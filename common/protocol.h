/**
 * @file    protocol.h
 * @brief   TCP communication protocol for the Student Database System.
 *
 * This header is the ONLY file shared between client and server that
 * defines the wire format. Both sides #include this file, ensuring
 * Request and Response sizes are always identical on both ends.
 *
 * Wire format (no framing, no length prefix needed):
 *   Client  -->  send(fd, &request,  sizeof(Request),  0)
 *   Server  -->  send(fd, &response, sizeof(Response), 0)
 *
 * All structs contain fixed-size fields ONLY (no pointers, no VLAs).
 * sizeof(Request) and sizeof(Response) are compile-time constants.
 *
 * Platform portability:
 *   Socket types and headers differ between Windows (Winsock2) and POSIX.
 *   Use the SOCKET_T typedef and INVALID_SOCK / SOCK_ERR macros from this
 *   header instead of raw int/SOCKET to write portable socket code.
 *
 * Server defaults:
 *   DEFAULT_PORT : 8080
 *   DEFAULT_HOST : "127.0.0.1"
 *
 * @author  Student Record System
 * @version 2.0.0  (client-server edition)
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "student.h"   /* Student, ResultCode */

/* =========================================================================
 * Platform-specific socket abstraction
 * ========================================================================= */

#ifdef _WIN32
    /* Windows — Winsock2
     * _WIN32_WINNT >= 0x0600 (Vista) is required for:
     *   - inet_ntop()   (converts binary IP to string)
     *   - getaddrinfo() (DNS resolution)
     * Old MinGW defaults to 0x0500 (Win2000) which hides these symbols.
     * Define it before any winsock header is pulled in. */
    #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
    #elif _WIN32_WINNT < 0x0600
    #undef  _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    /* Link against ws2_32.lib (Makefile handles this via -lws2_32) */

    typedef SOCKET          SOCKET_T;
    #define INVALID_SOCK    INVALID_SOCKET
    #define SOCK_ERR        SOCKET_ERROR
    #define CLOSE_SOCK(s)   closesocket(s)

    /** Initialise Winsock; call once at program start on Windows. */
    #define SOCK_INIT()     do { \
        WSADATA _wsa; \
        if (WSAStartup(MAKEWORD(2,2), &_wsa) != 0) { \
            fprintf(stderr, "[FATAL] WSAStartup failed: %d\n", WSAGetLastError()); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

    /** Clean up Winsock; call once at program end on Windows. */
    #define SOCK_CLEANUP()  WSACleanup()

#else
    /* POSIX — Linux / macOS */
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>

    typedef int             SOCKET_T;
    #define INVALID_SOCK    (-1)
    #define SOCK_ERR        (-1)
    #define CLOSE_SOCK(s)   close(s)
    #define SOCK_INIT()     /* no-op on POSIX */
    #define SOCK_CLEANUP()  /* no-op on POSIX */
#endif

/* =========================================================================
 * Server configuration defaults
 * ========================================================================= */

/** Default TCP port the server listens on. */
#define DEFAULT_PORT        8080

/** Default server hostname the client connects to. */
#define DEFAULT_HOST        "127.0.0.1"

/** Maximum simultaneous pending connections in the listen() queue. */
#define LISTEN_BACKLOG      10

/** Timeout for recv() on the server side (seconds). 0 = no timeout. */
#define SERVER_RECV_TIMEOUT 30

/* =========================================================================
 * Operation codes  (Client → Server, in Request.operation)
 * ========================================================================= */

/**
 * @brief  Identifies the requested database operation.
 *
 * Sent in every Request. The server's request_handler dispatches on this
 * value. Explicit numeric values are assigned to prevent ABI shifts if the
 * enum is extended.
 */
typedef enum {
    OP_ADD      = 1,   /**< Add a new student record.                        */
    OP_VIEW_ALL = 2,   /**< Retrieve all active student records.              */
    OP_SEARCH   = 3,   /**< Search (sub-type in Request.search_type).         */
    OP_UPDATE   = 4,   /**< Update an existing record (ID from Request.student.student_id). */
    OP_DELETE   = 5,   /**< Soft-delete a record by Request.student_id.       */
    OP_REPORT   = 6,   /**< Fetch aggregate statistics.                       */
    OP_PING     = 7,   /**< Connectivity check; server replies RESULT_OK.     */
    OP_QUIT     = 8    /**< Graceful disconnect notification.                 */
} OpCode;

/* =========================================================================
 * Search sub-types  (used when operation == OP_SEARCH)
 * ========================================================================= */

/**
 * @brief  Specifies which field to search on when OP_SEARCH is sent.
 */
typedef enum {
    SEARCH_BY_ID   = 1,   /**< Exact match on student_id (binary search).    */
    SEARCH_BY_NAME = 2,   /**< Substring match on name (linear search).      */
    SEARCH_BY_DEPT = 3    /**< Exact match on department (linear scan).       */
} SearchType;

/* =========================================================================
 * Sort keys  (sent in OP_VIEW_ALL to request sorted output)
 * ========================================================================= */

typedef enum {
    SORT_KEY_NONE  = 0,   /**< Return in storage order (default).            */
    SORT_KEY_NAME  = 1,
    SORT_KEY_GPA   = 2,
    SORT_KEY_ROLL  = 3,
    SORT_KEY_ID    = 4
} SortKey;

/* =========================================================================
 * Maximum records in a single Response
 * ========================================================================= */

/**
 * Maximum student records returned in one Response.students[] array.
 * Kept <= MAX_STUDENTS (500). Adjust if needed — both sides recompile.
 */
#define PROTO_MAX_RESPONSE_RECORDS   100

/* =========================================================================
 * Request  (Client → Server)
 * ========================================================================= */

/**
 * @brief  Fixed-size request packet sent from client to server.
 *
 * Usage per operation:
 *   OP_ADD      : populate .student with all fields.
 *   OP_VIEW_ALL : set .sort_key for desired order; other fields ignored.
 *   OP_SEARCH   : set .search_type + .student_id (BY_ID) or
 *                 .search_keyword (BY_NAME / BY_DEPT).
 *   OP_UPDATE   : populate .student (student_id identifies the record).
 *   OP_DELETE   : set .student_id.
 *   OP_REPORT   : no additional fields needed.
 *   OP_PING     : no additional fields needed.
 *   OP_QUIT     : no additional fields needed.
 */
typedef struct {
    int        operation;             /**< OpCode value.                      */
    int        search_type;           /**< SearchType (OP_SEARCH only).       */
    int        sort_key;              /**< SortKey (OP_VIEW_ALL only).        */
    int        student_id;            /**< Target ID (OP_DELETE, OP_SEARCH by ID). */
    char       search_keyword[64];    /**< Search term (OP_SEARCH by name/dept). */
    Student    student;               /**< Full record (OP_ADD, OP_UPDATE).   */
} Request;

/* =========================================================================
 * Response  (Server → Client)
 * ========================================================================= */

/**
 * @brief  Fixed-size response packet sent from server to client.
 *
 * The .status field holds a ResultCode value. The client should always
 * check status == RESULT_OK before accessing .students[] or statistics.
 *
 * Usage per operation:
 *   OP_ADD/UPDATE/DELETE : .status + .message only.
 *   OP_VIEW_ALL/SEARCH   : .status + .count + .students[0..count-1].
 *   OP_REPORT            : .status + .total_active + .avg_gpa + .top_gpa
 *                          + .dept_counts[] (index matches dept_names[]).
 *   OP_PING              : .status = RESULT_OK only.
 */
typedef struct {
    int        status;                          /**< ResultCode value.             */
    char       message[128];                    /**< Human-readable result string. */
    int        count;                           /**< Records in students[].        */
    Student    students[PROTO_MAX_RESPONSE_RECORDS]; /**< Returned records.        */
    /* Report statistics (populated for OP_REPORT) */
    float      avg_gpa;                         /**< Average GPA of all active.    */
    float      top_gpa;                         /**< Highest individual GPA.       */
    int        total_active;                    /**< Total active student count.   */
    char       top_student_name[MAX_NAME_LEN];  /**< Name of top-GPA student.      */
} Response;

/* =========================================================================
 * Helper — map OpCode to display string (for logging/debugging)
 * ========================================================================= */

/**
 * @brief  Return a human-readable name for an OpCode.
 * @param  op  The operation code.
 * @return Static NUL-terminated string. Never NULL.
 */
const char *opcode_to_string(int op);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
