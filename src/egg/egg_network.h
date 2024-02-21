/* egg_network.h
 */
 
#ifndef EGG_NETWORK_H
#define EGG_NETWORK_H

/* Check settings and driver status, how available do we think the network is?
 * Returns bitfields.
 * When "PLEASE_LIMIT" in play, don't send usage statistics, don't check for DLC, or any other non-essential things.
 * Your call whether high score lists are essential, I tend to think No.
 * Enforcing PLEASE_LIMIT is entirely up to you.
 */
int egg_network_probe();
#define EGG_NETWORK_POSSIBLE        0x00000001 /* Plumbing exists. */
#define EGG_NETWORK_AVAILABLE       0x00000002 /* We appear to be on-network and authorized to do at least something. */
#define EGG_NETWORK_HTTP_OK         0x00000004 /* HTTP should be available. */
#define EGG_NETWORK_WEBSOCKET_OK    0x00000008 /* WebSocket should be available. */
#define EGG_NETWORK_HOST_RESTRICT   0x00000010 /* Artificial restrictions in place, on which hosts can be connected. */
#define EGG_NETWORK_VOLUME_RESTRICT 0x00000020 /* Artificial bandwidth quotas in place. */
#define EGG_NETWORK_MOCK            0x00000040 /* Platform is only pretending to call out; network will not be touched at all. */
#define EGG_NETWORK_PLEASE_LIMIT    0x00000080 /* User requests essential use only. */

/* Begin an HTTP call.
 * Returns <0 if something goes wrong queuing the request; no response event will fire.
 * If this returns >=0, you will eventually receive one EGG_EVENT_TYPE_HTTP_RESPONSE with the same (userdata).
 * Response (status_code) will be zero for faulty responses, eg host not found.
 */
int egg_http(
  const char *host,
  int port,
  const char *path,
  const char **headers,
  const void *body,int bodyc,
  const void *userdata
);

/* Begin a WebSocket connection attempt.
 * Like egg_http(), this may fail immediately by returning <0,
 * otherwise you'll get events with the same (userdata):
 *   EGG_EVENT_WEBSOCKET_ERROR: Failed to connect, or abnormal termination after connected.
 *   EGG_EVENT_WEBSOCKET_CLOSE: Normal termination.
 *   EGG_EVENT_WEBSOCKET_CONNECT: Connection successful.
 *   EGG_EVENT_WEBSOCKET_MESSAGE: Packet received.
 * On success, we return a >0 websocketid that you can use to send messages or disconnect.
 * Each connected websocket will eventually cause exactly one ERROR or CLOSE event; that's the last time you'll see it.
 */
int egg_websocket_connect(
  const char *host,
  int port,
  const char *path,
  const char *protocol,
  const void *userdata
);

/* Drop a websocket if connected.
 * You will receive a CLOSE event, maybe during this call, or maybe after.
 */
void egg_websocket_disconnect(int websocketid);

/* Queue a packet for sending over websocket.
 * I/O does not necessarily happen during this call.
 * Immediately errors eg no such websocket, cause <0 here and no event.
 * I/O failure may be reported later as EGG_EVENT_WEBSOCKET_ERROR (and also will drop the websocket).
 * You do not have a good way to detect when a packet is actually sent over the wire.
 */
int egg_websocket_send(int websocketid,int opcode,const void *body,int bodyc);

#endif
