/**
 * @file    request_handler.h
 * @brief   TCP request dispatcher for the Student Database server.
 *
 * This module receives a connected client socket, reads one Request,
 * dispatches to the appropriate db_* function, populates a Response,
 * and sends it back. It knows nothing about file paths, registry internals,
 * or socket setup — those are owned by database.c and server.c respectively.
 *
 * Multithreading slot:
 *   handle_client() is designed to be passed directly to pthread_create()
 *   as the thread function when Phase 2 threading is added:
 *
 *       pthread_create(&tid, NULL, handle_client_thread, (void*)(intptr_t)fd);
 *
 *   A thin wrapper handle_client_thread() that casts the void* back to
 *   SOCKET_T and calls handle_client() is all that's needed.
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../common/protocol.h"   /* SOCKET_T, Request, Response, OpCode */

/**
 * @brief  Service a single connected client.
 *
 * Runs a receive-dispatch-send loop until the client sends OP_QUIT,
 * closes the connection, or a network error occurs.
 *
 * @param  client_fd  Connected client socket descriptor.
 */
void handle_client(SOCKET_T client_fd);

/**
 * @brief  Receive exactly sizeof(Request) bytes from a socket.
 *
 * Loops on recv() to handle short reads. Returns 1 on success, 0 on
 * connection close, -1 on error.
 *
 * @param  fd   Socket to read from.
 * @param  req  Output buffer. Must not be NULL.
 */
int recv_request(SOCKET_T fd, Request *req);

/**
 * @brief  Send exactly sizeof(Response) bytes on a socket.
 *
 * Loops on send() to handle short writes. Returns 1 on success, -1 on error.
 *
 * @param  fd    Socket to write to.
 * @param  resp  Response to send. Must not be NULL.
 */
int send_response(SOCKET_T fd, const Response *resp);

#ifdef __cplusplus
}
#endif

#endif /* REQUEST_HANDLER_H */
