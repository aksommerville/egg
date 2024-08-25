#include "http_internal.h"

/* Delete.
 */
 
void http_websocket_del(struct http_websocket *ws) {
  if (!ws) return;
  if (ws->cb) ws->cb(ws,-1,0,0);
  free(ws);
}

/* New.
 */
 
struct http_websocket *http_websocket_new(struct http_context *ctx) {
  struct http_websocket *ws=calloc(1,sizeof(struct http_websocket));
  if (!ws) return 0;
  ws->ctx=ctx;
  return ws;
}

/* Disconnect.
 */
 
void http_websocket_disconnect(struct http_websocket *ws) {
  if (!ws) return;
  struct http_socket *sock=http_context_socket_for_websocket(ws->ctx,ws);
  if (!sock) return;
  http_socket_force_defunct(sock);
}

/* Send.
 */

int http_websocket_send(struct http_websocket *ws,int opcode,const void *v,int c) {
  if (c<0) return -1;
  struct http_socket *sock=http_context_socket_for_websocket(ws->ctx,ws);
  if (!sock) return -1;
  
  // Preamble can go up to 10 bytes.
  char preamble[10];
  int preamblec=0;
  preamble[preamblec++]=0x80|(opcode&0x0f); // 0x80=terminator, we don't allow continued packets.
  if (c<0x7e) { // short
    preamble[preamblec++]=c;
  } else if (c<0x10000) { // medium
    preamble[preamblec++]=0x7e;
    preamble[preamblec++]=c>>8;
    preamble[preamblec++]=c;
  } else { // huge
    preamble[preamblec++]=0x7f;
    preamble[preamblec++]=0;
    preamble[preamblec++]=0;
    preamble[preamblec++]=0;
    preamble[preamblec++]=0;
    preamble[preamblec++]=c>>24;
    preamble[preamblec++]=c>>16;
    preamble[preamblec++]=c>>8;
    preamble[preamblec++]=c;
  }
  if (sr_encode_raw(&sock->wbuf,preamble,preamblec)<0) return -1;
  if (sr_encode_raw(&sock->wbuf,v,c)<0) return -1;

  return 0;
}

/* Trivial accessors.
 */

void *http_websocket_get_userdata(const struct http_websocket *ws) {
  return ws->userdata;
}

void http_websocket_set_userdata(struct http_websocket *ws,void *userdata) {
  ws->userdata=userdata;
}

void http_websocket_set_callback(struct http_websocket *ws,int (*cb)(struct http_websocket *ws,int opcode,const void *v,int c)) {
  ws->cb=cb;
}

/* Encode client-side upgrade request.
 */
 
int http_websocket_encode_upgrade_request(
  struct sr_encoder *dst,
  struct http_websocket *ws,
  const char *url,int urlc
) {
  struct http_url surl={0};
  if (http_url_split(&surl,url,urlc)<0) return -1;
  
  if (sr_encode_fmt(dst,"GET %.*s HTTP/1.1\r\n",surl.pathc+surl.queryc,surl.path)<0) return -1;
  if (sr_encode_raw(dst,"Connection: Upgrade\r\n",-1)<0) return -1;
  if (sr_encode_raw(dst,"Upgrade: websocket\r\n",-1)<0) return -1;
  // Sec-WebSocket-Key is supposed to be securely random, and we're supposed to validate it against Sec-WebSocket-Accept in the response.
  if (sr_encode_raw(dst,"Sec-WebSocket-Key: 1EmWXqGAJxTL/HSMhR8KKw==\r\n",-1)<0) return -1;
  if (sr_encode_raw(dst,"\r\n",2)<0) return -1;
  
  return 0;
}

/* Check upgrade.
 */
 
struct http_websocket *http_websocket_check_upgrade(struct http_xfer *req,struct http_xfer *rsp) {
  if (!req||!rsp) return 0;
  
  // Confirm that it is a WebSocket upgrade request. If not, no worries, we're done.
  const char *connection=0,*upgrade=0,*version=0;
  int connectionc=http_xfer_get_header(&connection,req,"Connection",10);
  if ((connectionc!=7)||sr_memcasecmp(connection,"Upgrade",7)) return 0;
  int upgradec=http_xfer_get_header(&upgrade,req,"Upgrade",7);
  if ((upgradec!=9)||sr_memcasecmp(upgrade,"websocket",9)) return 0;
  int versionc=http_xfer_get_header(&version,req,"Sec-WebSocket-Version",21);
  if (versionc<0) versionc=0;
  
  // Some wacky nonsense around "key"...
  const char *hkey=0;
  int hkeyc=http_xfer_get_header(&hkey,req,"Sec-WebSocket-Key",17);
  char prekey[256],digest[20];
  int prekeyc=hkeyc+36;
  if ((hkeyc<1)||(prekeyc>sizeof(prekey))) return 0;
  memcpy(prekey,hkey,hkeyc);
  memcpy(prekey+hkeyc,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",36);
  sr_sha1(digest,sizeof(digest),prekey,prekeyc);
  char postkey[256];
  int postkeyc=sr_base64_encode(postkey,sizeof(postkey),digest,20);
  if ((postkeyc<0)||(postkeyc>sizeof(postkey))) return 0;
  
  // Compose response.
  http_xfer_set_topline(rsp,"HTTP/1.1 101 Upgrade to WebSocket",-1);
  http_xfer_set_header(rsp,"Connection",10,"Upgrade",7);
  http_xfer_set_header(rsp,"Upgrade",7,"WebSocket",9);
  http_xfer_set_header(rsp,"Sec-WebSocket-Accept",20,postkey,postkeyc);
  
  // Find our socket and bump it to WebSocket mode.
  // We need to encode the response directly, since once in WebSocket mode, the socket won't.
  struct http_socket *sock=http_context_socket_for_request(req->ctx,req);
  if (!sock) return 0;
  if (http_xfer_encode(&sock->wbuf,rsp)<0) return 0;
  sock->role=HTTP_SOCKET_ROLE_WEBSOCKET;
  
  // And finally, create a WebSocket object on the socket for bookkeeping.
  if (!sock->ws) {
    if (!(sock->ws=http_websocket_new(sock->ctx))) {
      http_socket_force_defunct(sock);
      return 0;
    }
    // Probably we ought to capture some things from the request and store on sock->ws.
  }
  
  return sock->ws;
}
