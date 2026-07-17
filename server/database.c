/**
 * @file    database.c
 * @brief   Database abstraction layer implementation.
 *
 * Owns the single global StudentRegistry for the server process and wraps
 * student.c CRUD + file_manager.c I/O behind the db_* API.
 *
 * Threading note (Phase 1):
 *   Single-threaded. For Phase 2 multithreading, add:
 *       static pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
 *   and wrap each public function body with lock/unlock calls.
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "database.h"
#include "../common/protocol.h"   /* SortKey enum — SORT_KEY_* constants */
#include "file_manager.h"  /* fm_load_registry, fm_save_registry, fm_ensure_data_dir */
#include "search.h"        /* search_by_name, search_by_department, SearchResult */
#include "sort.h"          /* sort_by_name, sort_by_gpa, sort_by_roll, sort_by_id */


/* =========================================================================
 * Internal state — the single source of truth for the server process
 * ========================================================================= */

/** The in-memory registry. Owned exclusively by this module. */
static StudentRegistry g_registry;

/** 1 after db_init() succeeds; guards against calling CRUD before init. */
static int g_initialized = 0;

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

ResultCode db_init(void)
{
    ResultCode rc;

    /* Ensure data/ directory exists (creates if absent) */
    rc = fm_ensure_data_dir();
    if (rc != RESULT_OK) {
        fprintf(stderr, "[DB] Failed to ensure data directory: %s\n",
                result_to_string(rc));
        return rc;
    }

    /* Initialise registry to zeroed state */
    registry_init(&g_registry);

    /* Load persisted data (RESULT_OK even on first-run / missing file) */
    rc = fm_load_registry(&g_registry);
    if (rc != RESULT_OK) {
        fprintf(stderr, "[DB] Load failed: %s\n", result_to_string(rc));
        return rc;
    }

    g_initialized = 1;
    fprintf(stdout, "[DB] Initialised. %d active record(s) loaded.\n",
            g_registry.count);
    return RESULT_OK;
}

ResultCode db_shutdown(void)
{
    ResultCode rc = RESULT_OK;

    if (!g_initialized) {
        return RESULT_OK; /* nothing to do */
    }

    if (g_registry.is_dirty) {
        rc = fm_save_registry(&g_registry);
        if (rc != RESULT_OK) {
            fprintf(stderr, "[DB] Shutdown save failed: %s\n",
                    result_to_string(rc));
        } else {
            fprintf(stdout, "[DB] Registry saved on shutdown.\n");
        }
    }

    registry_destroy(&g_registry);
    g_initialized = 0;
    return rc;
}

ResultCode db_flush(void)
{
    if (!g_initialized) return RESULT_ERR_INVALID;

    ResultCode rc = fm_save_registry(&g_registry);
    if (rc == RESULT_OK) {
        fprintf(stdout, "[DB] Manual flush complete.\n");
    }
    return rc;
}

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/** Copy up to max_out active records from the registry into out_arr. */
static int copy_active_records(Student *out_arr, int max_out)
{
    int copied = 0;
    int i;
    for (i = 0; i < MAX_STUDENTS && copied < max_out; i++) {
        if (g_registry.records[i].is_active) {
            out_arr[copied++] = g_registry.records[i];
        }
    }
    return copied;
}

/** Auto-save after any mutation; log but don't abort on save failure. */
static ResultCode auto_save(void)
{
    ResultCode rc = fm_save_registry(&g_registry);
    if (rc != RESULT_OK) {
        fprintf(stderr, "[DB] Auto-save failed: %s\n", result_to_string(rc));
    }
    return rc;
}

/* =========================================================================
 * CRUD
 * ========================================================================= */

ResultCode db_add_student(const Student *s)
{
    if (!g_initialized) return RESULT_ERR_INVALID;
    if (!s)             return RESULT_ERR_INVALID;

    ResultCode rc = student_add(&g_registry, s);
    if (rc == RESULT_OK) {
        auto_save();
    }
    return rc;
}

ResultCode db_get_all(Student *out_arr, int *out_count, int sort_key)
{
    if (!g_initialized || !out_arr || !out_count) return RESULT_ERR_INVALID;

    /* Apply sort to the registry before copying */
    switch (sort_key) {
        case SORT_KEY_NAME: sort_by_name(&g_registry, SORT_ASCENDING);  break;
        case SORT_KEY_GPA:  sort_by_gpa (&g_registry, SORT_DESCENDING); break;
        case SORT_KEY_ROLL: sort_by_roll(&g_registry, SORT_ASCENDING);  break;
        case SORT_KEY_ID:   sort_by_id  (&g_registry);                   break;
        default: /* SORT_KEY_NONE — storage order */                      break;
    }

    *out_count = copy_active_records(out_arr, MAX_STUDENTS);
    return RESULT_OK;
}

ResultCode db_get_by_id(int id, Student *out)
{
    if (!g_initialized || !out) return RESULT_ERR_INVALID;
    return student_get_by_id(&g_registry, id, out);
}

ResultCode db_update_student(const Student *s)
{
    if (!g_initialized) return RESULT_ERR_INVALID;
    if (!s)             return RESULT_ERR_INVALID;

    ResultCode rc = student_update(&g_registry, s);
    if (rc == RESULT_OK) {
        auto_save();
    }
    return rc;
}

ResultCode db_delete_student(int id)
{
    if (!g_initialized) return RESULT_ERR_INVALID;

    ResultCode rc = student_delete(&g_registry, id);
    if (rc == RESULT_OK) {
        auto_save();
    }
    return rc;
}

/* =========================================================================
 * Search
 * ========================================================================= */

ResultCode db_search_by_name(const char *keyword, Student *out_arr, int *out_count)
{
    if (!g_initialized || !keyword || !out_arr || !out_count)
        return RESULT_ERR_INVALID;

    SearchResult result = {0};
    ResultCode rc = search_by_name(&g_registry, keyword, &result);

    if (rc == RESULT_OK) {
        int i;
        int n = (result.count > MAX_STUDENTS) ? MAX_STUDENTS : result.count;
        for (i = 0; i < n; i++) {
            out_arr[i] = g_registry.records[result.indices[i]];
        }
        *out_count = n;
    }

    search_result_free(&result);
    return rc;
}

ResultCode db_search_by_dept(const char *dept, Student *out_arr, int *out_count)
{
    if (!g_initialized || !dept || !out_arr || !out_count)
        return RESULT_ERR_INVALID;

    SearchResult result = {0};
    ResultCode rc = search_by_department(&g_registry, dept, &result);

    if (rc == RESULT_OK) {
        int i;
        int n = (result.count > MAX_STUDENTS) ? MAX_STUDENTS : result.count;
        for (i = 0; i < n; i++) {
            out_arr[i] = g_registry.records[result.indices[i]];
        }
        *out_count = n;
    }

    search_result_free(&result);
    return rc;
}

/* =========================================================================
 * Reports
 * ========================================================================= */

ResultCode db_report(float *avg_gpa, float *top_gpa, int *total,
                     Student *top_student)
{
    if (!g_initialized) return RESULT_ERR_INVALID;

    *total = g_registry.count;

    if (g_registry.count == 0) {
        *avg_gpa = 0.0f;
        *top_gpa = 0.0f;
        return RESULT_OK;
    }

    /* Average GPA */
    ResultCode rc = student_average_gpa(&g_registry, avg_gpa);
    if (rc != RESULT_OK) *avg_gpa = 0.0f;

    /* Top GPA student */
    Student top = {0};
    rc = student_get_top_gpa(&g_registry, &top);
    if (rc == RESULT_OK) {
        *top_gpa = top.gpa;
        if (top_student) {
            *top_student = top;
        }
    } else {
        *top_gpa = 0.0f;
    }

    return RESULT_OK;
}
