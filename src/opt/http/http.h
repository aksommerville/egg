/* http.h
 */
 
#ifndef HTTP_H
#define HTTP_H

struct http_context;
struct http_xfer;
struct http_websocket;
struct sr_encoder;
struct pollfd;

/* Context.
 * There should be just one for each program.
 * Incoming HTTP requests and connection and disconnect of Websockets are handled by a global delegate.
 * Incoming HTTP responses and WebSocket payloads are delivered to per-object callbacks.
 *****************************************************************************/
 
struct http_context_delegate {
  void *userdata;
  int (*cb_serve)(struct http_xfer *req,struct http_xfer *rsp,void *userdata);
  int (*cb_connect)(struct http_websocket *ws,void *userdata);
  int (*cb_disconnect)(struct http_websocket *ws,void *userdata);
};

void http_context_del(struct http_context *ctx);
struct http_context *http_context_new(const struct http_context_delegate *delegate);

struct http_limits {
  int requests; // Max HTTP requests in flight at a time. Doesn't count WebSockets.
  int websockets; // Max WebSockets at a time.
  int backlog; // Max outbound requests deferred due to (requests) limit.
  double idle_timeout; // sec. When to drop sockets, if they are between transactions.
  double active_timeout; // sec. Drop socket mid-transaction.
  int transfer_size; // Max size for open-ended constructions like HTTP headers.
  int body_size; // Max size for HTTP bodies and WebSocket packets.
};

void http_context_get_limits(struct http_limits *limits,const struct http_context *ctx);
int http_context_set_limits(struct http_context *ctx,const struct http_limits *limits);

/* Create or destroy a TCP server to receive incoming connections.
 */
int http_listen(struct http_context *ctx,int local_only,int port);
int http_unlisten(struct http_context *ctx,int port);

/* Most clients should use http_update().
 * That does its own poll() call, with the provided timeout.
 * If you want to combine our poll with something else, call http_get_files(),
 * then http_update_file() for everything that polled.
 */
int http_update(struct http_context *ctx,int toms);
int http_get_files(struct pollfd *dst,int dsta,struct http_context *ctx);
int http_update_file(struct http_context *ctx,int fd);

/* WebSocket.
 ****************************************************************/

/* Create a client WebSocket, connected to some remote host.
 * On success, returns a WEAK object owned by the context.
 * All WebSockets trigger their callback with (-1,0,0) as they delete.
 * You must drop any references to it at that time.
 */
struct http_websocket *http_websocket_connect(
  struct http_context *ctx,
  const char *url,int urlc,
  int (*cb)(struct http_websocket *ws,int opcode,const void *v,int c),
  void *userdata
);

/* Disconnect a WebSocket, either client or server role.
 * Global cb_disconnect will not fire for it.
 */
void http_websocket_disconnect(struct http_websocket *ws);

int http_websocket_send(struct http_websocket *ws,int opcode,const void *v,int c);

void *http_websocket_get_userdata(const struct http_websocket *ws);
void http_websocket_set_userdata(struct http_websocket *ws,void *userdata);
void http_websocket_set_callback(struct http_websocket *ws,int (*cb)(struct http_websocket *ws,int opcode,const void *v,int c));

struct http_websocket *http_context_get_websocket_by_index(const struct http_context *ctx,int p);

/* Convenience for WebSocket servers.
 * If this request asks for an upgrade to WebSocket, respond affirmatively and return the new WebSocket object.
 * Not-a-WebSocket-request is indistinguishable from real errors.
 */
struct http_websocket *http_websocket_check_upgrade(struct http_xfer *req,struct http_xfer *rsp);

/* xfer: Request or response.
 * This is a dumb object. Its only job is to model the HTTP request/response.
 * They are also used as identifiers for pending transactions.
 ******************************************************************/

/* Prepare an outgoing HTTP request.
 * The TCP connection may be established during this call, but no data is sent.
 * Context provides a request backlog, and only allows so many in flight at a time.
 * Caller is expected to provide headers and body after this succeeds.
 * Do not provide "Host" or "Content-Length" headers, those are automatic.
 * On success, returns a WEAK object owned by the context.
 */
struct http_xfer *http_request(
  struct http_context *ctx,
  const char *method,
  const char *url,int urlc,
  int (*cb)(struct http_xfer *req,struct http_xfer *rsp),
  void *userdata // lives on (req) at callback
);

void http_request_cancel(struct http_xfer *req);

void *http_xfer_get_userdata(const struct http_xfer *xfer);
void http_xfer_set_userdata(struct http_xfer *xfer,void *userdata);

int http_xfer_get_topline(void *dstpp,const struct http_xfer *xfer);
int http_xfer_set_topline(struct http_xfer *xfer,const char *src,int srcc);
int http_xfer_get_method(char *dst,int dsta,const struct http_xfer *xfer); // Forces to uppercase.
int http_xfer_get_path(void *dstpp,const struct http_xfer *xfer); // No query.
int http_xfer_get_query(void *dstpp,const struct http_xfer *xfer); // Trims leading '?'.
int http_xfer_get_status(const struct http_xfer *xfer);
int http_xfer_get_message(void *dstpp,const struct http_xfer *xfer);
int http_xfer_set_status(struct http_xfer *xfer,int status,const char *fmt,...);

int http_xfer_for_each_header(
  const struct http_xfer *xfer,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
);
int http_xfer_add_header(struct http_xfer *xfer,const char *k,int kc,const char *v,int vc); // Appends blindly.
int http_xfer_set_header(struct http_xfer *xfer,const char *k,int kc,const char *v,int vc); // Replace or append.
int http_xfer_get_header(void *dstpp,const struct http_xfer *xfer,const char *k,int kc); // First match only.
int http_xfer_get_header_int(int *v,const struct http_xfer *xfer,const char *k,int kc);

/* Includes body, if Content-Type is application/x-www-form-urlencoded or multipart/form-data.
 * Iteration does not decode keys or values.
 * Lookups do decode values for you, but keys must match exactly without decoding.
 */
int http_xfer_for_each_param(
  const struct http_xfer *xfer,
  int (*cb)(const char *k,int kc,const char *v,int vc,void *userdata),
  void *userdata
);
int http_xfer_get_param(char *dst,int dsta,const struct http_xfer *xfer,const char *k,int kc);
int http_xfer_get_param_int(int *v,const struct http_xfer *xfer,const char *k,int kc);

struct sr_encoder *http_xfer_get_body(struct http_xfer *xfer);

/* Assemble the full request or response, suitable for putting right on the wire.
 */
int http_xfer_encode(struct sr_encoder *dst,const struct http_xfer *xfer);

/* Read piecemeal off the wire.
 * http_xfer_decode() returns the length consumed, and updates internal state accordingly.
 * http_xfer_is_decoded() returns nonzero after a complete request or response has been decoded.
 * Once that happens, decode will never consume anything more.
 * is_decoded is also used as a signal for programmatically-populated xfers, that they're ready to encode.
 */
int http_xfer_decode(struct http_xfer *xfer,const void *src,int srcc);
int http_xfer_is_decoded(const struct http_xfer *xfer);

/* Odds, ends.
 ************************************************************/
 
/* Current real time in seconds.
 */
double http_now();

/* Length through first '\r\n', or zero if none.
 */
int http_measure_line(const char *src,int srcc);

/* (pat) may contain '*' for anything-but-slash and '**' for anything.
 * Leading and trailing slashes are ignored.
 * Otherwise must be an exact match.
 */
int http_path_match(const char *pat,int patc,const char *src,int srcc);

/* Break a URL into scheme, host, port, path, query, and fragment.
 * (port) is empty or should be a decimal integer.
 * (path) includes the leading slash.
 * (query) includes the leading question mark.
 * (fragment) includes the leading hash.
 * (scheme) does NOT include the trailing colon.
 * You can always safely read (path,pathc+queryc) to get the combined path as written in Request-Line.
 */
struct http_url {
  const char *scheme;
  int schemec;
  const char *host;
  int hostc;
  const char *port;
  int portc;
  const char *path;
  int pathc;
  const char *query;
  int queryc;
  const char *fragment;
  int fragmentc;
};
int http_url_split(struct http_url *dst,const char *src,int srcc);

/* Helper for cb_serve.
 * Variadic arguments are any number of:
 *   (int method,const char *path,int (*cb)(struct http_xfer *req,struct http_xfer *rsp))
 * Paths may contain '*' to match anything but slash or '**' to match anything including slash.
 * Method zero and empty path match all.
 * First match wins.
 * So it's normal to end with (0,"",cb_serve_default) if you have such a thing.
 */
int http_dispatch_(struct http_xfer *req,struct http_xfer *rsp,...);
#define http_dispatch(req,rsp,...) http_dispatch_(req,rsp,##__VA_ARGS__,-1)
#define HTTP_METHOD_GET 1
#define HTTP_METHOD_POST 2
#define HTTP_METHOD_PUT 3
#define HTTP_METHOD_DELETE 4
#define HTTP_METHOD_PATCH 5

#endif
