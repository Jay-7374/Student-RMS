/**
 * @file    client.c
 * @brief   TCP client connection implementation.
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "../common/protocol.h"

/* =========================================================================
 * Connection lifecycle
 * ========================================================================= */

int client_connect(ClientConn *conn, const char *host, int port)
{
    struct sockaddr_in  server_addr;
    SOCKET_T            fd;

    memset(conn, 0, sizeof(ClientConn));
    strncpy(conn->host, host, sizeof(conn->host) - 1);
    conn->port      = port;
    conn->connected = 0;
    conn->fd        = INVALID_SOCK;

    SOCK_INIT();

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCK) {
        fprintf(stderr, "[Client] socket() failed.\n");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons((unsigned short)port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "[Client] Invalid address: %s\n", host);
        CLOSE_SOCK(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCK_ERR) {
        fprintf(stderr, "[Client] connect() failed. Is the server running?\n");
        CLOSE_SOCK(fd);
        return -1;
    }

    conn->fd        = fd;
    conn->connected = 1;
    fprintf(stdout, "[Client] Connected to %s:%d\n", host, port);
    return 0;
}

void client_disconnect(ClientConn *conn)
{
    if (!conn->connected) return;

    /* Send OP_QUIT so the server can clean up its session */
    Request quit_req = {0};
    quit_req.operation = OP_QUIT;
    Response resp = {0};
    client_exchange(conn, &quit_req, &resp);

    CLOSE_SOCK(conn->fd);
    conn->fd        = INVALID_SOCK;
    conn->connected = 0;
    SOCK_CLEANUP();
    fprintf(stdout, "[Client] Disconnected from server.\n");
}

int client_is_connected(const ClientConn *conn)
{
    return conn && conn->connected;
}

/* =========================================================================
 * Send / receive
 * ========================================================================= */

int client_send(ClientConn *conn, const Request *req)
{
    size_t      total  = 0;
    size_t      needed = sizeof(Request);
    const char *buf    = (const char *)req;
    int         n;

    if (!conn->connected) return -1;

    while (total < needed) {
        n = (int)send(conn->fd, buf + total, (int)(needed - total), 0);
        if (n <= 0) {
            fprintf(stderr, "[Client] send() failed.\n");
            conn->connected = 0;
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

int client_recv(ClientConn *conn, Response *resp)
{
    size_t  total  = 0;
    size_t  needed = sizeof(Response);
    char   *buf    = (char *)resp;
    int     n;

    if (!conn->connected) return -1;

    memset(resp, 0, sizeof(Response));

    while (total < needed) {
        n = (int)recv(conn->fd, buf + total, (int)(needed - total), 0);
        if (n <= 0) {
            if (n == 0)
                fprintf(stderr, "[Client] Server closed connection unexpectedly.\n");
            else
                fprintf(stderr, "[Client] recv() failed.\n");
            conn->connected = 0;
            return -1;
        }
        total += (size_t)n;
    }
    return 0;
}

int client_exchange(ClientConn *conn, const Request *req, Response *resp)
{
    if (client_send(conn, req) != 0) return -1;
    if (client_recv(conn, resp) != 0) return -1;
    return 0;
}
