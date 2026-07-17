/**
 * @file    client.h
 * @brief   TCP client connection API for the Student Database System.
 *
 * Provides the ClientConn handle that wraps the TCP socket, and the
 * client_send / client_recv helpers that ensure complete sends and receives
 * (handling short reads/writes internally).
 *
 * Usage pattern:
 *
 *     ClientConn conn;
 *     if (client_connect(&conn, "127.0.0.1", 8080) != 0) { ... }
 *
 *     Request  req  = {0};
 *     Response resp = {0};
 *     req.operation = OP_PING;
 *     client_send(&conn, &req);
 *     client_recv(&conn, &resp);
 *
 *     client_disconnect(&conn);
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#ifndef CLIENT_H
#define CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../common/protocol.h"   /* SOCKET_T, Request, Response */

/* =========================================================================
 * Connection handle
 * ========================================================================= */

/**
 * @brief  Opaque connection context for a client session.
 *
 * Pass a pointer to this struct to all client_* functions.
 * Do not access fields directly.
 */
typedef struct {
    SOCKET_T  fd;               /**< Connected socket descriptor.   */
    char      host[64];         /**< Server hostname / IP.          */
    int       port;             /**< Server TCP port.               */
    int       connected;        /**< 1 if currently connected.      */
} ClientConn;

/* =========================================================================
 * Connection lifecycle
 * ========================================================================= */

/**
 * @brief  Connect to the Student Database server.
 *
 * Creates a TCP socket, resolves the host, and calls connect(). On success
 * conn->connected is set to 1.
 *
 * @param  conn  Uninitialised ClientConn to populate. Must not be NULL.
 * @param  host  Server hostname or IP string. Must not be NULL.
 * @param  port  Server TCP port.
 * @return 0 on success, -1 on failure (error printed to stderr).
 */
int client_connect(ClientConn *conn, const char *host, int port);

/**
 * @brief  Send OP_QUIT and close the TCP connection.
 *
 * Sends a graceful OP_QUIT request so the server can clean up, then closes
 * the socket. Safe to call even if already disconnected.
 *
 * @param  conn  Active connection. Must not be NULL.
 */
void client_disconnect(ClientConn *conn);

/* =========================================================================
 * Send / receive
 * ========================================================================= */

/**
 * @brief  Send a Request to the server (reliable, handles short writes).
 *
 * @param  conn  Active connection. Must not be NULL.
 * @param  req   Request to send. Must not be NULL.
 * @return 0 on success, -1 on network error (connection marked disconnected).
 */
int client_send(ClientConn *conn, const Request *req);

/**
 * @brief  Receive a Response from the server (reliable, handles short reads).
 *
 * @param  conn  Active connection. Must not be NULL.
 * @param  resp  Output buffer. Must not be NULL.
 * @return 0 on success, -1 on network error or server disconnect.
 */
int client_recv(ClientConn *conn, Response *resp);

/**
 * @brief  Send a Request and immediately receive a Response.
 *
 * Convenience wrapper that calls client_send() then client_recv().
 *
 * @param  conn  Active connection.
 * @param  req   Request to send.
 * @param  resp  Output buffer for the server's response.
 * @return 0 on success, -1 on any failure.
 */
int client_exchange(ClientConn *conn, const Request *req, Response *resp);

/* =========================================================================
 * Utility
 * ========================================================================= */

/**
 * @brief  Check whether the connection is alive.
 * @param  conn  Connection to check. Must not be NULL.
 * @return 1 if connected, 0 otherwise.
 */
int client_is_connected(const ClientConn *conn);

#ifdef __cplusplus
}
#endif

#endif /* CLIENT_H */
