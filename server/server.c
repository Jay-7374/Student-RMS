/**
 * @file    server.c
 * @brief   TCP server entry point for the Student Database System.
 *
 * Responsibilities:
 *   - Initialise Winsock (Windows) / no-op (POSIX).
 *   - Initialise the database layer (loads students.dat).
 *   - Create, bind, and listen on a TCP socket.
 *   - Accept client connections in a loop.
 *   - Dispatch each connection to handle_client() in request_handler.c.
 *   - Gracefully shut down on SIGINT/SIGTERM.
 *
 * Phase 2 multithreading slot:
 *   Replace the direct handle_client(client_fd) call with:
 *
 *       pthread_t tid;
 *       SOCKET_T *pfd = malloc(sizeof(SOCKET_T));
 *       *pfd = client_fd;
 *       pthread_create(&tid, NULL, handle_client_thread, pfd);
 *       pthread_detach(tid);
 *
 *   Where handle_client_thread(void *arg) casts arg to SOCKET_T* and
 *   calls handle_client(), then free(arg). No other changes needed.
 *
 * Usage:
 *   server.exe [port]          (default port: 8080)
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "../common/protocol.h"    /* SOCKET_T, SOCK_INIT, SOCK_CLEANUP, DEFAULT_PORT */
#include "database.h"              /* db_init, db_shutdown */
#include "request_handler.h"       /* handle_client */

/* =========================================================================
 * Graceful shutdown via signal handler
 * ========================================================================= */

/** Set to 1 by SIGINT/SIGTERM handler to break the accept loop. */
static volatile int g_running = 1;

/** The listening socket — closed by signal handler for clean exit. */
static SOCKET_T g_listen_fd = INVALID_SOCK;

static void signal_handler(int signum)
{
    (void)signum;
    fprintf(stdout, "\n[Server] Shutdown signal received. Stopping...\n");
    g_running = 0;
    /* Closing the listen socket unblocks accept() on most platforms */
    if (g_listen_fd != INVALID_SOCK) {
        CLOSE_SOCK(g_listen_fd);
        g_listen_fd = INVALID_SOCK;
    }
}

/* =========================================================================
 * Server setup helpers
 * ========================================================================= */

/**
 * @brief  Create and configure the server listening socket.
 *
 * @param  port  TCP port to bind to.
 * @return Valid SOCKET_T on success, INVALID_SOCK on failure.
 */
static SOCKET_T create_listen_socket(int port)
{
    SOCKET_T          fd;
    struct sockaddr_in addr;
    int                opt = 1;

    /* Create TCP socket */
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCK) {
        fprintf(stderr, "[Server] socket() failed.\n");
        return INVALID_SOCK;
    }

    /* SO_REUSEADDR — allow immediate re-bind after restart */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                   (const char *)&opt, sizeof(opt)) == SOCK_ERR) {
        fprintf(stderr, "[Server] setsockopt(SO_REUSEADDR) failed.\n");
        CLOSE_SOCK(fd);
        return INVALID_SOCK;
    }

    /* Bind to all interfaces on the specified port */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((unsigned short)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCK_ERR) {
        fprintf(stderr, "[Server] bind() failed on port %d.\n", port);
        CLOSE_SOCK(fd);
        return INVALID_SOCK;
    }

    /* Start listening */
    if (listen(fd, LISTEN_BACKLOG) == SOCK_ERR) {
        fprintf(stderr, "[Server] listen() failed.\n");
        CLOSE_SOCK(fd);
        return INVALID_SOCK;
    }

    return fd;
}

/* =========================================================================
 * main()
 * ========================================================================= */

int main(int argc, char *argv[])
{
    int      port = DEFAULT_PORT;
    SOCKET_T client_fd;
    struct   sockaddr_in client_addr;
    int      client_addr_len = sizeof(client_addr);

#ifdef _WIN32
    /* Switch the Windows console to UTF-8 so box-drawing characters
     * (╔ ═ ║ ╚ etc.) render correctly without needing 'chcp 65001'. */
    SetConsoleOutputCP(65001);
#endif

    /* Optional command-line port override */
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "[Server] Invalid port: %s\n", argv[1]);
            return EXIT_FAILURE;
        }
    }

    /* ── Banner ─────────────────────────────────────────────────── */
    fprintf(stdout,
            "╔══════════════════════════════════════════╗\n"
            "║  Student Database System  —  SERVER      ║\n"
            "║  Version 2.0.0  |  Port %-5d            ║\n"
            "╚══════════════════════════════════════════╝\n\n",
            port);

    /* ── Platform socket init ────────────────────────────────────── */
    SOCK_INIT();

    /* ── Signal handlers ────────────────────────────────────────── */
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    /* ── Database init ──────────────────────────────────────────── */
    fprintf(stdout, "[Server] Initialising database...\n");
    if (db_init() != RESULT_OK) {
        fprintf(stderr, "[Server] Database init failed. Exiting.\n");
        SOCK_CLEANUP();
        return EXIT_FAILURE;
    }

    /* ── Create listening socket ────────────────────────────────── */
    g_listen_fd = create_listen_socket(port);
    if (g_listen_fd == INVALID_SOCK) {
        fprintf(stderr, "[Server] Failed to create listen socket. Exiting.\n");
        db_shutdown();
        SOCK_CLEANUP();
        return EXIT_FAILURE;
    }

    fprintf(stdout, "[Server] Listening on 0.0.0.0:%d ...\n", port);
    fprintf(stdout, "[Server] Press Ctrl+C to stop.\n\n");

    /* ── Accept loop ─────────────────────────────────────────────── */
    while (g_running) {
        memset(&client_addr, 0, sizeof(client_addr));

        client_fd = accept(g_listen_fd,
                           (struct sockaddr *)&client_addr,
                           (socklen_t *)&client_addr_len);

        if (client_fd == INVALID_SOCK) {
            if (!g_running) break; /* Interrupted by signal — expected */
            fprintf(stderr, "[Server] accept() error. Continuing.\n");
            continue;
        }

        /* Log incoming connection */
        {
            char client_ip[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &client_addr.sin_addr,
                      client_ip, sizeof(client_ip));
            fprintf(stdout, "[Server] Connection from %s:%d\n",
                    client_ip, ntohs(client_addr.sin_port));
        }

        /*
         * Phase 1 — iterative (one client at a time):
         *   handle_client() blocks until the client disconnects.
         *
         * Phase 2 — multithreaded (TODO):
         *   pthread_create(&tid, NULL, handle_client_thread, pfd);
         */
        handle_client(client_fd);
        /* client_fd is closed inside handle_client() */
    }

    /* ── Cleanup ────────────────────────────────────────────────── */
    fprintf(stdout, "[Server] Shutting down database...\n");
    db_shutdown();

    SOCK_CLEANUP();
    fprintf(stdout, "[Server] Goodbye.\n");
    return EXIT_SUCCESS;
}
