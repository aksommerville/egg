#include "http_internal.h"
#include <sys/time.h>

/* Current time.
 */
 
double http_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  return (double)tv.tv_sec+(double)tv.tv_usec/1000000.0;
}

/* Delete.
 */

void http_context_del(struct http_context *ctx) {
  if (!ctx) return;
  if (ctx->socketv) {
    while (ctx->socketc-->0) {
      http_socket_del(ctx->socketv[ctx->socketc]);
    }
    free(ctx->socketv);
  }
  if (ctx->backlogv) {
    while (ctx->backlogc-->0) {
      http_xfer_del(ctx->backlogv[ctx->backlogc]);
    }
    free(ctx->backlogv);
  }
  if (ctx->pollfdv) free(ctx->pollfdv);
  free(ctx);
}

/* New.
 */
 
struct http_context *http_context_new(const struct http_context_delegate *delegate) {
  struct http_context *ctx=calloc(1,sizeof(struct http_context));
  if (!ctx) return 0;
  if (delegate) ctx->delegate=*delegate;
  
  ctx->limits.requests=10;
  ctx->limits.websockets=10;
  ctx->limits.backlog=100;
  ctx->limits.idle_timeout=10.0;
  ctx->limits.active_timeout=60.0;
  ctx->limits.transfer_size=8192;
  ctx->limits.body_size=25<<20;
  
  return ctx;
}

/* Trivial accessors.
 */

void http_context_get_limits(struct http_limits *limits,const struct http_context *ctx) {
  *limits=ctx->limits;
}

int http_context_set_limits(struct http_context *ctx,const struct http_limits *limits) {
  // Opportunity to validate and reject. But I think anything goes.
  ctx->limits=*limits;
  return 0;
}

/* Grow socketv if needed.
 */
 
static int http_context_socketv_require(struct http_context *ctx) {
  if (ctx->socketc<ctx->socketa) return 0;
  int na=ctx->socketa=16;
  if (na>INT_MAX/sizeof(void*)) return -1;
  void *nv=realloc(ctx->socketv,sizeof(void*)*na);
  if (!nv) return -1;
  ctx->socketv=nv;
  ctx->socketa=na;
  return 0;
}

/* Count of HTTP streaming sockets, both incoming and outgoing.
 * Idle streams don't count.
 */
 
static int http_context_count_requests(const struct http_context *ctx) {
  int reqc=0;
  struct http_socket **sockp=ctx->socketv;
  int i=ctx->socketc;
  for (;i-->0;sockp++) {
    switch ((*sockp)->role) {
      case HTTP_SOCKET_ROLE_SERVER_STREAM:
      case HTTP_SOCKET_ROLE_CLIENT_STREAM:
        break;
      default: continue;
    }
    if ((*sockp)->state==HTTP_STREAM_STATE_IDLE) continue;
    reqc++;
  }
  return reqc;
}

/* Find socket by request xfer.
 */
 
struct http_socket *http_context_socket_for_request(
  const struct http_context *ctx,
  const struct http_xfer *req
) {
  struct http_socket **sockp=ctx->socketv;
  int i=ctx->socketc;
  for (;i-->0;sockp++) {
    if ((*sockp)->req==req) return *sockp;
  }
  return 0;
}

struct http_socket *http_context_socket_for_websocket(
  const struct http_context *ctx,
  const struct http_websocket *ws
) {
  struct http_socket **sockp=ctx->socketv;
  int i=ctx->socketc;
  for (;i-->0;sockp++) {
    if ((*sockp)->ws==ws) return *sockp;
  }
  return 0;
}

/* Add server.
 */

int http_listen(struct http_context *ctx,int local_only,int port) {
  local_only=local_only?1:0;

  /* Check whether we're already listening on this port.
   */
  int i=ctx->socketc;
  while (i-->0) {
    struct http_socket *sock=ctx->socketv[i];
    if (sock->role!=HTTP_SOCKET_ROLE_SERVER) continue;
    if (sock->port!=port) continue;
    if (sock->local_only==local_only) return 0; // Already got it.
    // Must close this one and reopen.
    ctx->socketc--;
    memmove(ctx->socketv+i,ctx->socketv+i+1,sizeof(void*)*(ctx->socketc-i));
    http_socket_del(sock);
    break;
  }

  if (http_context_socketv_require(ctx)<0) return -1;
  struct http_socket *sock=http_socket_new(ctx);
  if (!sock) return -1;
  if (http_socket_configure_server(sock,local_only,port)<0) {
    http_socket_del(sock);
    return -1;
  }
  ctx->socketv[ctx->socketc++]=sock;
  return 0;
}

/* Remove server.
 */
 
int http_unlisten(struct http_context *ctx,int port) {
  int i=ctx->socketc;
  while (i-->0) {
    struct http_socket *sock=ctx->socketv[i];
    if (sock->role!=HTTP_SOCKET_ROLE_SERVER) continue;
    if (sock->port!=port) continue;
    ctx->socketc--;
    memmove(ctx->socketv+i,ctx->socketv+i+1,sizeof(void*)*(ctx->socketc-i));
    http_socket_del(sock);
    return 0;
  }
  return -1;
}

/* Update, we poll.
 */

int http_update(struct http_context *ctx,int toms) {
  int pollfdc=0;
  for (;;) {
    if ((pollfdc=http_get_files(ctx->pollfdv,ctx->pollfda,ctx))<0) return -1;
    if (pollfdc<=ctx->pollfda) break;
    int na=(pollfdc+16)&~15;
    if (na>INT_MAX/sizeof(struct pollfd)) return -1;
    void *nv=realloc(ctx->pollfdv,sizeof(struct pollfd)*na);
    if (!nv) return -1;
    ctx->pollfdv=nv;
    ctx->pollfda=na;
  }
  if (pollfdc<=0) {
    if (toms>0) usleep(toms*1000);
    return 0;
  }
  int err=poll(ctx->pollfdv,pollfdc,toms);
  if (err<0) return 0;
  if (!err) return 0;
  const struct pollfd *pollfd=ctx->pollfdv;
  int i=pollfdc;
  for (;i-->0;pollfd++) {
    if (!pollfd->revents) continue;
    if (http_update_file(ctx,pollfd->fd)<0) return -1;
  }
  return 1;
}

/* List pollable files.
 * This is also the general non-I/O update.
 */
  
int http_get_files(struct pollfd *dst,int dsta,struct http_context *ctx) {

  double now=http_now();

  int dstc=0,i=ctx->socketc;
  while (i-->0) {
    struct http_socket *sock=ctx->socketv[i];
    
    if (http_socket_preupdate(sock)<0) {
      http_socket_force_defunct(sock);
    }
    
    if (http_socket_is_defunct(sock,now)) {
      ctx->socketc--;
      memmove(ctx->socketv+i,ctx->socketv+i+1,sizeof(void*)*(ctx->socketc-i));
      http_socket_del(sock);
      continue;
    }
    
    if (dstc<dsta) {
      struct pollfd *pollfd=dst+dstc++;
      memset(pollfd,0,sizeof(struct pollfd));
      pollfd->fd=sock->fd;
      if (sock->wbufp<sock->wbuf.c) {
        pollfd->events=POLLOUT|POLLERR|POLLHUP;
      } else {
        pollfd->events=POLLIN|POLLERR|POLLHUP;
      }
    } else {
      dstc++;
    }
  }
  return dstc;
}

/* Update one polled file.
 */
 
int http_update_file(struct http_context *ctx,int fd) {
  int i=ctx->socketc;
  while (i-->0) {
    struct http_socket *sock=ctx->socketv[i];
    if (sock->fd!=fd) continue;
    if (http_socket_update(sock)<0) {
      ctx->socketc--;
      memmove(ctx->socketv+i,ctx->socketv+i+1,sizeof(void*)*(ctx->socketc-i));
      http_socket_del(sock);
    }
    return 0;
  }
  return -1;
}

/* Find websocket.
 */
 
struct http_websocket *http_context_get_websocket_by_index(const struct http_context *ctx,int p) {
  if (p<0) return 0;
  struct http_socket **sock=ctx->socketv;
  int i=ctx->socketc;
  for (;i-->0;sock++) {
    if ((*sock)->role!=HTTP_SOCKET_ROLE_WEBSOCKET) continue;
    if (!(*sock)->ws) continue;
    if (p--) continue;
    return (*sock)->ws;
  }
  return 0;
}

/* New WebSocket client connection.
 */
 
struct http_websocket *http_websocket_connect(
  struct http_context *ctx,
  const char *url,int urlc,
  int (*cb)(struct http_websocket *ws,int opcode,const void *v,int c),
  void *userdata
) {
  if (ctx->limits.websockets<1) return 0;
  if (http_context_socketv_require(ctx)<0) return 0;
  struct http_socket *sock=http_socket_new(ctx);
  if (!sock) return 0;
  if (http_socket_configure_websocket_client(sock,url,urlc)<0) {
    http_socket_del(sock);
    return 0;
  }
  sock->ws->cb=cb;
  sock->ws->userdata=userdata;
  ctx->socketv[ctx->socketc++]=sock;
  return sock->ws;
}

/* Handoff client request xfer to backlog.
 */
 
static int http_context_handoff_backlog(struct http_context *ctx,struct http_xfer *req) {
  if (ctx->backlogc>=ctx->limits.backlog) return -1;
  if (ctx->backlogc>=ctx->backloga) {
    int na=ctx->backloga+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(ctx->backlogv,sizeof(void*)*na);
    if (!nv) return -1;
    ctx->backlogv=nv;
    ctx->backloga=na;
  }
  ctx->backlogv[ctx->backlogc++]=req;
  return 0;
}

/* Handoff client request to a new socket, and register that socket.
 * Or reuse an idle socket if one exists.
 */
 
static struct http_socket *http_context_handoff_request(struct http_context *ctx,struct http_xfer *req,const char *url,int urlc) {

  int i=ctx->socketc;
  while (i-->0) {
    struct http_socket *sock=ctx->socketv[i];
    if (sock->role!=HTTP_SOCKET_ROLE_CLIENT_STREAM) continue;
    if (sock->state!=HTTP_STREAM_STATE_IDLE) continue;
    if (http_socket_is_defunct(sock,-1.0)) continue;
    if (http_socket_configure_client_stream(sock,req,url,urlc)<0) return 0;
    return sock;
  }

  if (http_context_socketv_require(ctx)<0) return 0;
  struct http_socket *sock=http_socket_new(ctx);
  if (!sock) return 0;
  if (http_socket_configure_client_stream(sock,req,url,urlc)<0) {
    http_socket_del(sock);
    return 0;
  }
  ctx->socketv[ctx->socketc++]=sock;
  return sock;
}

/* New HTTP client request.
 */
 
struct http_xfer *http_request(
  struct http_context *ctx,
  const char *method,
  const char *url,int urlc,
  int (*cb)(struct http_xfer *req,struct http_xfer *rsp),
  void *userdata
) {
  if (ctx->limits.requests<1) return 0;
  
  struct http_xfer *req=http_xfer_new(ctx);
  if (!req) return 0;
  if (http_xfer_configure_client_request(req,method,url,urlc,cb,userdata)<0) {
    http_xfer_del(req);
    return 0;
  }
  
  int requestc=http_context_count_requests(ctx);
  if (requestc>=ctx->limits.requests) {
    if (http_context_handoff_backlog(ctx,req)<0) {
      http_xfer_del(req);
      return 0;
    }

  } else {
    struct http_socket *sock=http_context_handoff_request(ctx,req,url,urlc);
    if (!sock) {
      http_xfer_del(req);
      return 0;
    }
  }
  return req;
}

/* New HTTP server request.
 */
 
struct http_socket *http_context_add_server_stream(
  struct http_context *ctx,
  int rfd,
  const void *raddr,int raddrc,
  struct http_socket *server
) {
  if (ctx->limits.requests<1) return 0;
  if (http_context_count_requests(ctx)>=ctx->limits.requests) return 0;
  if (http_context_socketv_require(ctx)<0) return 0;
  struct http_socket *sock=http_socket_new_handoff(ctx,rfd);
  if (!sock) return 0;
  if (http_socket_configure_server_stream(sock)<0) {
    sock->fd=-1; // revert handoff
    return 0;
  }
  ctx->socketv[ctx->socketc++]=sock;
  return sock;
}
