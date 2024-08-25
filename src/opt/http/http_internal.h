#ifndef HTTP_INTERNAL_H
#define HTTP_INTERNAL_H

#include "http.h"
#include "opt/serial/serial.h"
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#if USE_mswin
  #include <winsock2.h>
  //TODO Our API expects poll() to exist. We'll need to rephrase with select() somehow.
#else
  #include <sys/poll.h>
  #include <sys/socket.h>
  #include <netdb.h>
#endif

/* Context.
 ************************************************************/

struct http_context {
  struct http_context_delegate delegate;
  struct http_limits limits;
  struct http_socket **socketv;
  int socketc,socketa;
  struct http_xfer **backlogv; // Outbound requests not initiated yet.
  int backlogc,backloga;
  struct pollfd *pollfdv;
  int pollfda;
};

struct http_socket *http_context_add_server_stream(
  struct http_context *ctx,
  int rfd,
  const void *raddr,int raddrc,
  struct http_socket *server
);

struct http_socket *http_context_socket_for_request(
  const struct http_context *ctx,
  const struct http_xfer *req
);

struct http_socket *http_context_socket_for_websocket(
  const struct http_context *ctx,
  const struct http_websocket *ws
);

/* Socket.
 **************************************************************/

#define HTTP_SOCKET_ROLE_UNSET 0
#define HTTP_SOCKET_ROLE_SERVER 1
#define HTTP_SOCKET_ROLE_SERVER_STREAM 2
#define HTTP_SOCKET_ROLE_CLIENT_STREAM 3
#define HTTP_SOCKET_ROLE_WEBSOCKET 4

/* State for SERVER_STREAM and CLIENT_STREAM.
 * Both rest in IDLE state between transactions.
 * SERVER: RCVSTATUS => RCVHDR => RCVBODY? => SERVE? => SEND
 * CLIENT: GATHER => SEND => RCVSTATUS => RCVHDR => RCVBODY?
 */
#define HTTP_STREAM_STATE_IDLE      0 /* Awaiting request, free to drop. */
#define HTTP_STREAM_STATE_RCVHDR    1 /* Receiving headers. */
#define HTTP_STREAM_STATE_RCVBODY   2 /* Receiving body. */
#define HTTP_STREAM_STATE_SERVE     3 /* Awaiting deferred service. */
#define HTTP_STREAM_STATE_SEND      4 /* Sending response or request. */
#define HTTP_STREAM_STATE_RCVSTATUS 5 /* Awaiting response from remote. */
#define HTTP_STREAM_STATE_GATHER    6 /* Awaiting final outgoing request. */

struct http_socket {
  struct http_context *ctx;
  int fd;
  int role; // HTTP_SOCKET_ROLE_*
  int state; // STREAM only
  int local_only; // SERVER only
  int port; // SERVER only
  void *saddr; // struct sockaddr. Local for SERVER, remote for others.
  int saddrc;
  double activity_time;
  char *hoststr; // "HOST:PORT" for streams
  int hoststrc;
  int chunked;
  int expectc;
  int awaiting_upgrade;
  
  /* Read and write buffers.
   * When (wbufp<wbuf.c), we must write out before doing anything else.
   */
  struct sr_encoder rbuf;
  int rbufp;
  struct sr_encoder wbuf;
  int wbufp;
  
  /* High-level context objects. These are how we communicate beyond the http unit.
   * (ws) must always be set if our role is WEBSOCKET.
   * (req,rsp) are set transiently in both STREAM roles, according to state.
   */
  struct http_websocket *ws;
  struct http_xfer *req;
  struct http_xfer *rsp;
};

void http_socket_del(struct http_socket *sock);

/* A new socket is defunct by default, until you configure it.
 */
struct http_socket *http_socket_new(struct http_context *ctx);
struct http_socket *http_socket_new_handoff(struct http_context *ctx,int fd);

/* There is no "configure_websocket_server".
 * Those sockets begin as "server_stream" and get upgraded.
 */
int http_socket_configure_server(struct http_socket *sock,int local_only,int port);
int http_socket_configure_server_stream(struct http_socket *sock); // Must have initialized with handoff.
int http_socket_configure_client_stream(struct http_socket *sock,struct http_xfer *req,const char *url,int urlc); // (req) HANDOFF
int http_socket_configure_websocket_client(struct http_socket *sock,const char *url,int urlc);

/* (now<0.0) to skip timeout checks.
 */
int http_socket_is_defunct(const struct http_socket *sock,double now);
void http_socket_force_defunct(struct http_socket *sock);

/* All sockets must preupdate each cycle.
 * Those that poll will also get a regular update.
 */
int http_socket_preupdate(struct http_socket *sock);
int http_socket_update(struct http_socket *sock);

/* WebSocket.
 ***************************************************************/
 
struct http_websocket {
  struct http_context *ctx;
  int (*cb)(struct http_websocket *ws,int opcode,const void *v,int c);
  void *userdata;
};

void http_websocket_del(struct http_websocket *ws);
struct http_websocket *http_websocket_new(struct http_context *ctx);

int http_websocket_encode_upgrade_request(
  struct sr_encoder *dst,
  struct http_websocket *ws,
  const char *url,int urlc
);

/* Transfer.
 ****************************************************************/
 
struct http_xfer {
  struct http_context *ctx;
  struct sr_encoder body;
  int (*cb)(struct http_xfer *req,struct http_xfer *rsp);
  void *userdata;
  char *topline;
  int toplinec;
  struct http_header {
    char *v;
    int c;
    int kc;
  } *headerv;
  int headerc,headera;
};

void http_xfer_del(struct http_xfer *xfer);

struct http_xfer *http_xfer_new(struct http_context *ctx);

int http_xfer_configure_client_request(
  struct http_xfer *xfer,
  const char *method,
  const char *url,int urlc,
  int (*cb)(struct http_xfer *req,struct http_xfer *rsp),
  void *userdata
);

#endif
