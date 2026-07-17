/**
 * @file    request_handler.c
 * @brief   TCP request dispatcher implementation.
 *
 * Reads one Request per loop iteration, calls the correct db_* function,
 * builds a Response, and sends it. The dispatch switch is the single place
 * where OpCodes are mapped to database operations.
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "request_handler.h"
#include "database.h"
#include "../common/protocol.h"
#include "../common/student.h"

/* =========================================================================
 * Reliable send / receive helpers
 * ========================================================================= */

int recv_request(SOCKET_T fd, Request *req)
{
    size_t  total  = 0;
    size_t  needed = sizeof(Request);
    char   *buf    = (char *)req;
    int     n;

    while (total < needed) {
        n = (int)recv(fd, buf + total, (int)(needed - total), 0);
        if (n <= 0) {
            /* 0 = graceful close, <0 = error */
            return (n == 0) ? 0 : -1;
        }
        total += (size_t)n;
    }
    return 1;
}

int send_response(SOCKET_T fd, const Response *resp)
{
    size_t      total  = 0;
    size_t      needed = sizeof(Response);
    const char *buf    = (const char *)resp;
    int         n;

    while (total < needed) {
        n = (int)send(fd, buf + total, (int)(needed - total), 0);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return 1;
}

/* =========================================================================
 * Per-operation handlers (static — private to this file)
 * ========================================================================= */

static void handle_add(const Request *req, Response *resp)
{
    ResultCode rc = db_add_student(&req->student);
    resp->status  = (int)rc;
    strncpy(resp->message,
            rc == RESULT_OK ? "Student added successfully."
                            : result_to_string(rc),
            sizeof(resp->message) - 1);
}

static void handle_view_all(const Request *req, Response *resp)
{
    Student  tmp[MAX_STUDENTS];
    int      count = 0;
    int      i;

    ResultCode rc = db_get_all(tmp, &count, req->sort_key);
    resp->status  = (int)rc;

    if (rc != RESULT_OK) {
        strncpy(resp->message, result_to_string(rc), sizeof(resp->message) - 1);
        return;
    }

    /* Cap at PROTO_MAX_RESPONSE_RECORDS per packet */
    resp->count = (count > PROTO_MAX_RESPONSE_RECORDS)
                    ? PROTO_MAX_RESPONSE_RECORDS : count;

    for (i = 0; i < resp->count; i++) {
        resp->students[i] = tmp[i];
    }

    snprintf(resp->message, sizeof(resp->message),
             "%d record(s) retrieved.", resp->count);
}

static void handle_search(const Request *req, Response *resp)
{
    Student    tmp[MAX_STUDENTS];
    int        count = 0;
    ResultCode rc    = RESULT_ERR_INVALID;
    int        i;

    switch (req->search_type) {
        case SEARCH_BY_ID: {
            Student found = {0};
            rc = db_get_by_id(req->student_id, &found);
            if (rc == RESULT_OK) {
                tmp[0] = found;
                count  = 1;
            }
            break;
        }
        case SEARCH_BY_NAME:
            rc = db_search_by_name(req->search_keyword, tmp, &count);
            break;
        case SEARCH_BY_DEPT:
            rc = db_search_by_dept(req->search_keyword, tmp, &count);
            break;
        default:
            rc = RESULT_ERR_INVALID;
            break;
    }

    resp->status = (int)rc;

    if (rc != RESULT_OK) {
        strncpy(resp->message, result_to_string(rc), sizeof(resp->message) - 1);
        return;
    }

    resp->count = (count > PROTO_MAX_RESPONSE_RECORDS)
                    ? PROTO_MAX_RESPONSE_RECORDS : count;

    for (i = 0; i < resp->count; i++) {
        resp->students[i] = tmp[i];
    }

    snprintf(resp->message, sizeof(resp->message),
             "%d match(es) found.", resp->count);
}

static void handle_update(const Request *req, Response *resp)
{
    ResultCode rc = db_update_student(&req->student);
    resp->status  = (int)rc;
    strncpy(resp->message,
            rc == RESULT_OK ? "Student updated successfully."
                            : result_to_string(rc),
            sizeof(resp->message) - 1);
}

static void handle_delete(const Request *req, Response *resp)
{
    ResultCode rc = db_delete_student(req->student_id);
    resp->status  = (int)rc;
    strncpy(resp->message,
            rc == RESULT_OK ? "Student deleted successfully."
                            : result_to_string(rc),
            sizeof(resp->message) - 1);
}

static void handle_report(const Request *req, Response *resp)
{
    (void)req; /* unused */

    Student top = {0};
    ResultCode rc = db_report(&resp->avg_gpa, &resp->top_gpa,
                               &resp->total_active, &top);
    resp->status  = (int)rc;

    if (rc == RESULT_OK) {
        strncpy(resp->top_student_name, top.name,
                sizeof(resp->top_student_name) - 1);
        snprintf(resp->message, sizeof(resp->message),
                 "Report: %d active students, avg GPA %.2f, top GPA %.2f.",
                 resp->total_active, resp->avg_gpa, resp->top_gpa);
    } else {
        strncpy(resp->message, result_to_string(rc), sizeof(resp->message) - 1);
    }
}

static void handle_ping(const Request *req, Response *resp)
{
    (void)req;
    resp->status = (int)RESULT_OK;
    strncpy(resp->message, "PONG", sizeof(resp->message) - 1);
}

/* =========================================================================
 * Dispatcher
 * ========================================================================= */

static void dispatch(const Request *req, Response *resp)
{
    /* Zero the response so no stale data leaks to the client */
    memset(resp, 0, sizeof(Response));

    fprintf(stdout, "[Handler] Received: %s\n", opcode_to_string(req->operation));

    switch (req->operation) {
        case OP_ADD:      handle_add     (req, resp); break;
        case OP_VIEW_ALL: handle_view_all(req, resp); break;
        case OP_SEARCH:   handle_search  (req, resp); break;
        case OP_UPDATE:   handle_update  (req, resp); break;
        case OP_DELETE:   handle_delete  (req, resp); break;
        case OP_REPORT:   handle_report  (req, resp); break;
        case OP_PING:     handle_ping    (req, resp); break;
        case OP_QUIT:
            resp->status = (int)RESULT_OK;
            strncpy(resp->message, "Goodbye.", sizeof(resp->message) - 1);
            break;
        default:
            resp->status = (int)RESULT_ERR_INVALID;
            snprintf(resp->message, sizeof(resp->message),
                     "Unknown operation code: %d", req->operation);
            fprintf(stderr, "[Handler] Unknown opcode %d\n", req->operation);
            break;
    }
}

/* =========================================================================
 * Client session loop
 * ========================================================================= */

void handle_client(SOCKET_T client_fd)
{
    Request  req;
    Response resp;
    int      result;

    fprintf(stdout, "[Server] Client connected. Waiting for requests...\n");

    while (1) {
        memset(&req, 0, sizeof(req));

        result = recv_request(client_fd, &req);
        if (result == 0) {
            fprintf(stdout, "[Server] Client disconnected gracefully.\n");
            break;
        }
        if (result < 0) {
            fprintf(stderr, "[Server] recv() error. Closing connection.\n");
            break;
        }

        /* Dispatch and build response */
        dispatch(&req, &resp);

        /* Send response */
        if (send_response(client_fd, &resp) < 0) {
            fprintf(stderr, "[Server] send() error. Closing connection.\n");
            break;
        }

        /* Terminate loop on OP_QUIT */
        if (req.operation == OP_QUIT) {
            fprintf(stdout, "[Server] Client sent OP_QUIT. Closing session.\n");
            break;
        }
    }

    CLOSE_SOCK(client_fd);
}
