#include "http_internal.h"

/* Delete.
 */

void http_socket_del(struct http_socket *sock) {
  if (!sock) return;
  if (sock->fd>=0) close(sock->fd);
  if (sock->saddr) free(sock->saddr);
  if (sock->hoststr) free(sock->hoststr);
  sr_encoder_cleanup(&sock->rbuf);
  sr_encoder_cleanup(&sock->wbuf);
  http_websocket_del(sock->ws);
  http_xfer_del(sock->req);
  http_xfer_del(sock->rsp);
  free(sock);
}

/* New socket.
 */
 
struct http_socket *http_socket_new(struct http_context *ctx) {
  struct http_socket *sock=calloc(1,sizeof(struct http_socket));
  if (!sock) return 0;
  sock->fd=-1;
  sock->ctx=ctx;
  sock->activity_time=http_now();
  return sock;
}

struct http_socket *http_socket_new_handoff(struct http_context *ctx,int fd) {
  if (fd<0) return 0;
  struct http_socket *sock=calloc(1,sizeof(struct http_socket));
  if (!sock) return 0;
  sock->fd=fd;
  sock->ctx=ctx;
  sock->activity_time=http_now();
  return sock;
}

/* Populate (fd,saddr) for local interfaces.
 * Do not bind.
 */
 
static int http_socket_newfd_local(struct http_socket *sock,int local_only,int port) {
  struct addrinfo *ai0=0;
  struct addrinfo hints={
    .ai_socktype=SOCK_STREAM,
    .ai_flags=AI_PASSIVE|AI_ADDRCONFIG,
  };
  const char *hoststr=0;
  if (local_only) {
    hints.ai_flags&=~AI_ADDRCONFIG;
    hoststr="localhost";
  }
  char portstr[32];
  snprintf(portstr,sizeof(portstr),"%d",port);
  if (getaddrinfo(hoststr,portstr,&hints,&ai0)<0) return -1;
  struct addrinfo *ai=ai0;
  for (;ai;ai=ai->ai_next) {
    // Any need to filter addresses here?
  
    void *nsaddr=malloc(ai->ai_addrlen);
    if (!nsaddr) {
      freeaddrinfo(ai0);
      return -1;
    }
    memcpy(nsaddr,ai->ai_addr,ai->ai_addrlen);
    if (sock->saddr) free(sock->saddr);
    sock->saddr=nsaddr;
    sock->saddrc=ai->ai_addrlen;
    
    if ((sock->fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol))<0) {
      freeaddrinfo(ai0);
      return -1;
    }
    freeaddrinfo(ai0);
    return 0;
  }
  freeaddrinfo(ai0);
  return -1;
}

/* Populate (fd,saddr,hoststr) for remote hosts.
 * Do not connect.
 */
 
static int http_socket_newfd_remote(struct http_socket *sock,const char *url,int urlc) {
  struct http_url surl={0};
  if (http_url_split(&surl,url,urlc)<0) return -1;
  struct addrinfo *ai0=0;
  struct addrinfo hints={
    .ai_socktype=SOCK_STREAM,
    .ai_flags=0,
  };
  char hoststr[256];
  if (surl.hostc>=sizeof(hoststr)) return -1;
  memcpy(hoststr,surl.host,surl.hostc);
  hoststr[surl.hostc]=0;
  char portstr[32];
  if (surl.portc>0) {
    if (surl.portc>=sizeof(portstr)) return -1;
    memcpy(portstr,surl.port,surl.portc);
    portstr[surl.portc]=0;
  } else if (surl.schemec>0) {
    if (surl.schemec>=sizeof(portstr)) return -1;
    memcpy(portstr,surl.scheme,surl.schemec);
    portstr[surl.schemec]=0;
  } else {
    memcpy(portstr,"80",3);
  }
  
  { // Set hoststr.
    int portstrc=0;
    while (portstr[portstrc]) portstrc++;
    int nc=surl.hostc+1+portstrc;
    char *nv=malloc(nc+1);
    if (!nv) return -1;
    memcpy(nv,hoststr,surl.hostc);
    nv[surl.hostc]=':';
    memcpy(nv+surl.hostc+1,portstr,portstrc);
    nv[nc]=0;
    if (sock->hoststr) free(sock->hoststr);
    sock->hoststr=nv;
    sock->hoststrc=nc;
  }
  
  if (getaddrinfo(hoststr,portstr,&hints,&ai0)<0) return -1;
  struct addrinfo *ai=ai0;
  for (;ai;ai=ai->ai_next) {
    //TODO Filter addresses?
  
    void *nsaddr=malloc(ai->ai_addrlen);
    if (!nsaddr) {
      freeaddrinfo(ai0);
      return -1;
    }
    memcpy(nsaddr,ai->ai_addr,ai->ai_addrlen);
    if (sock->saddr) free(sock->saddr);
    sock->saddr=nsaddr;
    sock->saddrc=ai->ai_addrlen;
    
    if ((sock->fd=socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol))<0) {
      freeaddrinfo(ai0);
      return -1;
    }
    freeaddrinfo(ai0);
    return 0;
  }
  freeaddrinfo(ai0);
  return -1;
}

/* Open a new server.
 */
 
int http_socket_configure_server(struct http_socket *sock,int local_only,int port) {
  if (sock->fd>=0) {
    close(sock->fd);
    sock->fd=-1;
  }
  if (http_socket_newfd_local(sock,local_only,port)<0) return -1;
  int one=1;
  setsockopt(sock->fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
  if (bind(sock->fd,(struct sockaddr*)sock->saddr,sock->saddrc)<0) return -1;
  if (listen(sock->fd,10)<0) return -1;
  sock->role=HTTP_SOCKET_ROLE_SERVER;
  sock->local_only=local_only;
  sock->port=port;
  return 0;
}

/* Begin server-side stream.
 */
 
int http_socket_configure_server_stream(struct http_socket *sock) {
  if (sock->fd<0) return -1; // Must create with fd handoff.
  sock->role=HTTP_SOCKET_ROLE_SERVER_STREAM;
  sock->state=HTTP_STREAM_STATE_IDLE;
  return 0;
}

/* Begin client-side stream.
 */

int http_socket_configure_client_stream(struct http_socket *sock,struct http_xfer *req,const char *url,int urlc) {
  if (sock->fd>=0) {
    close(sock->fd);
    sock->fd=-1;
  }
  if (http_socket_newfd_remote(sock,url,urlc)<0) return -1;
  if (connect(sock->fd,(struct sockaddr*)sock->saddr,sock->saddrc)<0) return -1;
  sock->role=HTTP_SOCKET_ROLE_CLIENT_STREAM;
  sock->state=HTTP_STREAM_STATE_GATHER;
  sock->req=req;
  // NB (req) is not yet ready to encode.
  return 0;
}

/* Begin client-side WebSocket.
 */
 
int http_socket_configure_websocket_client(struct http_socket *sock,const char *url,int urlc) {
  if (sock->fd>=0) {
    close(sock->fd);
    sock->fd=-1;
  }
  if (http_socket_newfd_remote(sock,url,urlc)<0) return -1;
  if (connect(sock->fd,(struct sockaddr*)sock->saddr,sock->saddrc)<0) return -1;
  if (!(sock->ws=http_websocket_new(sock->ctx))) return -1;
  if (http_websocket_encode_upgrade_request(&sock->wbuf,sock->ws,url,urlc)<0) return -1;
  sock->role=HTTP_SOCKET_ROLE_WEBSOCKET;
  sock->awaiting_upgrade=1;
  return 0;
}

/* Test defunct.
 */

int http_socket_is_defunct(const struct http_socket *sock,double now) {
  if (!sock) return 1;
  if (sock->fd<0) return 1;
  if (now>0.0) {
    double elapsed=now-sock->activity_time;
    if ((sock->state==HTTP_STREAM_STATE_IDLE)&&((sock->role==HTTP_SOCKET_ROLE_CLIENT_STREAM)||(sock->role==HTTP_SOCKET_ROLE_SERVER_STREAM))) {
      if (sock->ctx->limits.idle_timeout>0.0) {
        if (elapsed>=sock->ctx->limits.idle_timeout) return 1;
      }
    } else if (sock->role!=HTTP_SOCKET_ROLE_SERVER) {
      if (sock->ctx->limits.active_timeout>0.0) {
        if (elapsed>=sock->ctx->limits.active_timeout) return 1;
      }
    }
  }
  return 0;
}

/* Force defunct.
 */
 
void http_socket_force_defunct(struct http_socket *sock) {
  if ((sock->role==HTTP_SOCKET_ROLE_WEBSOCKET)&&sock->ws&&sock->ctx->delegate.cb_disconnect) {
    sock->ctx->delegate.cb_disconnect(sock->ws,sock->ctx->delegate.userdata);
    http_websocket_del(sock->ws);
    sock->ws=0;
    sock->role=HTTP_SOCKET_ROLE_UNSET;
  }
  if (sock->fd>=0) {
    close(sock->fd);
    sock->fd=-1;
  }
}

/* React to I/O error.
 * eg connection closed.
 */
 
static int http_socket_io_error(struct http_socket *sock) {
  http_socket_force_defunct(sock);
  return 0;
}

static int http_socket_delivery_error(struct http_socket *sock) {
  http_socket_force_defunct(sock);
  return 0;
}

static int http_socket_http_error(struct http_socket *sock) {
  // Better to populate (sock->rsp) with status 500 if feasible.
  http_socket_force_defunct(sock);
  return 0;
}

/* End HTTP transaction.
 */
 
static int http_socket_end_transaction(struct http_socket *sock) {
  http_xfer_del(sock->req);
  http_xfer_del(sock->rsp);
  sock->req=0;
  sock->rsp=0;
  sock->state=HTTP_STREAM_STATE_IDLE;
  return 0;
}

/* React to clearing the write buffer.
 */
 
static int http_socket_write_complete(struct http_socket *sock) {
  if (sock->role==HTTP_SOCKET_ROLE_SERVER_STREAM) {
    if (sock->state==HTTP_STREAM_STATE_SEND) {
      http_socket_end_transaction(sock);
    }
  }
  if (sock->role==HTTP_SOCKET_ROLE_CLIENT_STREAM) {
    if (sock->state==HTTP_STREAM_STATE_SEND) {
      sock->state=HTTP_STREAM_STATE_RCVSTATUS;
    }
  }
  return 0;
}

/* Inbound request is complete. Kick off the response.
 */
 
static int http_socket_call_for_service(struct http_socket *sock) {
  if (sock->ctx->delegate.cb_serve) {
    int err=sock->ctx->delegate.cb_serve(sock->req,sock->rsp,sock->ctx->delegate.userdata);
    if (err<0) return http_socket_http_error(sock);
  } else {
    return http_socket_http_error(sock);
  }
  return 0;
}

/* Receive Status Line or Request Line.
 * In IDLE state, consume any whitespace then snap into RCVSTATUS state.
 */
 
static int http_socket_deliver_HTTP_RCVSTATUS(struct http_socket *sock,const char *src,int srcc) {
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  if (srcp) return srcp;
  if ((srcp=http_measure_line(src,srcc))<1) {
    if (srcc>sock->ctx->limits.transfer_size) return -1;
    return 0;
  }
  switch (sock->role) {
    case HTTP_SOCKET_ROLE_SERVER_STREAM: {
        if (!sock->req) {
          if (!(sock->req=http_xfer_new(sock->ctx))) return -1;
        }
        if (http_xfer_set_topline(sock->req,src,srcp)<0) return -1;
      } break;
    case HTTP_SOCKET_ROLE_CLIENT_STREAM: {
        if (!sock->rsp) {
          if (!(sock->rsp=http_xfer_new(sock->ctx))) return -1;
        }
        if (http_xfer_set_topline(sock->rsp,src,srcp)<0) return -1;
      } break;
  }
  sock->state=HTTP_STREAM_STATE_RCVHDR;
  return srcp;
}

static int http_socket_deliver_HTTP_IDLE(struct http_socket *sock,const char *src,int srcc) {
  if ((unsigned char)src[0]>0x20) sock->state=HTTP_STREAM_STATE_RCVSTATUS;
  return http_socket_deliver_HTTP_RCVSTATUS(sock,src,srcc);
}

/* End of body.
 */
 
static int http_socket_finished_body(struct http_socket *sock) {
  switch (sock->role) {
    case HTTP_SOCKET_ROLE_SERVER_STREAM: {
        if (sock->rsp) return -1;
        if (!(sock->rsp=http_xfer_new(sock->ctx))) return -1;
        sock->state=HTTP_STREAM_STATE_SERVE;
        if (http_socket_call_for_service(sock)<0) return -1;
      } break;
    case HTTP_SOCKET_ROLE_CLIENT_STREAM: {
        if (sock->req->cb) {
          int err=sock->req->cb(sock->req,sock->rsp);
          if (err<0) return err;
        }
        http_socket_end_transaction(sock);
      } break;
  }
  return 0;
}
      

/* End of headers.
 * Enter body state if necessary.
 */
 
static int http_socket_finished_headers(struct http_socket *sock) {
  struct http_xfer *xfer=0;
  switch (sock->role) {
    case HTTP_SOCKET_ROLE_SERVER_STREAM: xfer=sock->req; break;
    case HTTP_SOCKET_ROLE_CLIENT_STREAM: xfer=sock->rsp; break;
  }
  if (!xfer) return -1;
  int content_length=0;
  http_xfer_get_header_int(&content_length,xfer,"Content-Length",14);
  if (content_length>0) {
    sock->state=HTTP_STREAM_STATE_RCVBODY;
    sock->chunked=0;
    sock->expectc=content_length;
    return 0;
  }
  const char *encoding=0;
  int encodingc=http_xfer_get_header(&encoding,xfer,"Transfer-Encoding",17);
  if ((encodingc==7)&&!memcmp(encoding,"chunked",7)) {
    sock->state=HTTP_STREAM_STATE_RCVBODY;
    sock->chunked=1;
    sock->expectc=0;
    return 0;
  }
  return http_socket_finished_body(sock);
}

/* Receive HTTP headers.
 */
 
static int http_socket_deliver_HTTP_RCVHDR(struct http_socket *sock,const char *src,int srcc) {
  int err=http_measure_line(src,srcc);
  if (err<1) {
    if (srcc>sock->ctx->limits.transfer_size) return -1;
    return 0;
  }
  srcc=err;
  int srcp=0;
  while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
  const char *k=src+srcp,*v=0;
  int kc=0,vc=0;
  while ((srcp<srcc)&&(src[srcp]!=':')) { srcp++; kc++; }
  while (kc&&((unsigned char)k[kc-1]<=0x20)) kc--;
  if ((srcp<srcc)&&(src[srcp]==':')) {
    srcp++;
    while ((srcp<srcc)&&((unsigned char)src[srcp]<=0x20)) srcp++;
    v=src+srcp;
    vc=srcc-srcp;
    while (vc&&((unsigned char)v[vc-1]<=0x20)) vc--;
  }
  if (!kc&&!vc) {
    if (http_socket_finished_headers(sock)<0) return -1;
    return srcc;
  }
  switch (sock->role) {
    case HTTP_SOCKET_ROLE_SERVER_STREAM: if (http_xfer_add_header(sock->req,k,kc,v,vc)<0) return -1; break;
    case HTTP_SOCKET_ROLE_CLIENT_STREAM: if (http_xfer_add_header(sock->rsp,k,kc,v,vc)<0) return -1; break;
    default: return -1;
  }
  return srcc;
}

/* Receive HTTP body.
 */
 
static int http_socket_deliver_HTTP_RCVBODY(struct http_socket *sock,const char *src,int srcc) {
  struct http_xfer *xfer=0;
  switch (sock->role) {
    case HTTP_SOCKET_ROLE_SERVER_STREAM: xfer=sock->req; break;
    case HTTP_SOCKET_ROLE_CLIENT_STREAM: xfer=sock->rsp; break;
  }
  if (!xfer) return -1;
  if (sock->expectc) {
    int cpc=sock->expectc;
    if (cpc>srcc) cpc=srcc;
    if (sr_encode_raw(&xfer->body,src,cpc)<0) return -1;
    if (xfer->body.c>sock->ctx->limits.body_size) return -1;
    sock->expectc-=cpc;
    if (!sock->expectc) {
      if (!sock->chunked) {
        if (http_socket_finished_body(sock)<0) return -1;
      }
    }
    return cpc;
  }
  if (!sock->chunked) return -1;
  if ((srcc=http_measure_line(src,srcc))<1) return 0;
  int srcp=0;
  for (;srcp<srcc;srcp++) {
    int digit=sr_digit_eval(src[srcp]);
    if ((digit>=0)&&(digit<0x10)) {
      if (sock->expectc&0xf8000000) return -1;
      sock->expectc<<=4;
      sock->expectc|=digit;
    }
  }
  if (!sock->expectc) {
    if (http_socket_finished_body(sock)<0) return -1;
  }
  return srcc;
}

/* Receive WebSocket packet.
 * (src) is deliberately not "const" -- if masked, we rewrite it in place.
 */
 
static int http_socket_deliver_WEBSOCKET(struct http_socket *sock,uint8_t *src,int srcc) {

  if (sock->awaiting_upgrade) {
    // We really ought to read the upgrade response and confirm that the server is willing to upgrade.
    // But we'll just assume that it's cool, and if the server chokes on the WebSocket packets we send it later, well, it should have listened.
    // (ditto if we choke on HTTP packets the server sends later...)
    if ((srcc>=4)&&!memcmp(src,"\r\n\r\n",4)) {
      sock->awaiting_upgrade=0;
      return 4;
    }
    int srcp=0;
    while ((srcp<srcc)&&(src[srcp]!='\r')) srcp++;
    if (srcp) return srcp;
    if (srcc>=4) return 1;
    return 0;
  }
  
  // Fixed-length preamble.
  if (srcc<2) return 0;
  uint8_t flags=src[0]&0xf0;
  uint8_t opcode=src[0]&0x0f;
  uint8_t hasmask=src[1]&0x80;
  const uint8_t *mask=0;
  int len=src[1]&0x7f;
  int srcp=2;
  
  if (!(flags&0x80)) {
    fprintf(stderr,"WebSocket packet with continuation: We don't support this.\n");
    return -1;
  }
  
  // Length.
  if (len==0x7e) {
    if (srcc<4) return 0;
    len=(src[2]<<8)|src[3];
    srcp=4;
  } else if (len==0x7f) {
    if (srcc<10) return 0;
    if (memcmp(src+2,"\0\0\0\0\0",5)) return -1; // >24-bit length, forget that.
    len=(src[7]<<16)|(src[8]<<8)|src[9];
    srcp=10;
  }
  
  // Mask.
  if (hasmask) {
    if (srcp>srcc-4) return 0;
    mask=src+srcp;
    srcp+=4;
  }
  
  // Payload.
  if (srcp>srcc-len) return 0;
  uint8_t *body=src+srcp;
  srcp+=len;
  
  // Apply mask.
  if (hasmask) {
    int i=0; for (;i<len;i++) body[i]^=mask[i&3];
  }
  
  // Send to delegate.
  if (sock->ws&&sock->ws->cb) {
    if (sock->ws->cb(sock->ws,opcode,body,len)<0) return -1;
  }
  
  return srcp;
}

/* Receive data.
 */
 
static int http_socket_deliver(struct http_socket *sock,const char *src,int srcc) {
  switch (sock->role) {
    case HTTP_SOCKET_ROLE_SERVER_STREAM:
    case HTTP_SOCKET_ROLE_CLIENT_STREAM: switch (sock->state) {
        case HTTP_STREAM_STATE_IDLE: return http_socket_deliver_HTTP_IDLE(sock,src,srcc);
        case HTTP_STREAM_STATE_RCVSTATUS: return http_socket_deliver_HTTP_RCVSTATUS(sock,src,srcc);
        case HTTP_STREAM_STATE_RCVHDR: return http_socket_deliver_HTTP_RCVHDR(sock,src,srcc);
        case HTTP_STREAM_STATE_RCVBODY: return http_socket_deliver_HTTP_RCVBODY(sock,src,srcc);
      } break;
    case HTTP_SOCKET_ROLE_WEBSOCKET: return http_socket_deliver_WEBSOCKET(sock,(uint8_t*)src,srcc);
  }
  return 0;
}

/* Preupdate.
 */
 
int http_socket_preupdate(struct http_socket *sock) {
  if (sock->fd<0) return 0;
  switch (sock->role) {
  
    case HTTP_SOCKET_ROLE_SERVER_STREAM: {
        if (sock->state==HTTP_STREAM_STATE_SERVE) {
          if (http_xfer_is_decoded(sock->rsp)) {
            if (http_xfer_encode(&sock->wbuf,sock->rsp)<0) return -1;
            sock->state=HTTP_STREAM_STATE_SEND;
          }
        }
      } break;
      
    case HTTP_SOCKET_ROLE_CLIENT_STREAM: {
        if (sock->state==HTTP_STREAM_STATE_GATHER) {
          if (http_xfer_is_decoded(sock->req)) {
            if (http_xfer_set_header(sock->req,"Host",4,sock->hoststr,sock->hoststrc)<0) return -1;
            if (http_xfer_encode(&sock->wbuf,sock->req)<0) return -1;
            sock->state=HTTP_STREAM_STATE_SEND;
          }
        }
      } break;
      
  }
  return 0;
}

/* Update.
 */
 
int http_socket_update(struct http_socket *sock) {
  if (sock->fd<0) return -1;
  
  if (sock->wbufp<sock->wbuf.c) {
    int err=write(sock->fd,(char*)sock->wbuf.v+sock->wbufp,sock->wbuf.c-sock->wbufp);
    if (err<=0) return http_socket_io_error(sock);
    sock->activity_time=http_now();
    if ((sock->wbufp+=err)>=sock->wbuf.c) {
      sock->wbufp=0;
      sock->wbuf.c=0;
      return http_socket_write_complete(sock);
    }
    
  } else if (sock->role==HTTP_SOCKET_ROLE_SERVER) {
    char raddr[256];
    socklen_t raddrc=sizeof(raddr);
    int rfd=accept(sock->fd,(struct sockaddr*)raddr,&raddrc);
    if (rfd<=0) return http_socket_io_error(sock);
    sock->activity_time=http_now();
    struct http_socket *stream=http_context_add_server_stream(sock->ctx,rfd,raddr,raddrc,sock);
    if (!stream) {
      close(rfd);
      return -1;
    }
    
  } else {
    if (sr_encoder_require(&sock->rbuf,1024)<0) return -1;
    int err=read(sock->fd,(char*)sock->rbuf.v+sock->rbuf.c,sock->rbuf.a-sock->rbuf.c);
    if (err<=0) return http_socket_io_error(sock);
    sock->activity_time=http_now();
    sock->rbuf.c+=err;
    while (sock->rbufp<sock->rbuf.c) {
      err=http_socket_deliver(sock,(char*)sock->rbuf.v+sock->rbufp,sock->rbuf.c-sock->rbufp);
      if (err<0) return http_socket_delivery_error(sock);
      if (!err) break;
      if ((sock->rbufp+=err)>=sock->rbuf.c) {
        sock->rbufp=0;
        sock->rbuf.c=0;
      }
    }
  }
  return 0;
}
