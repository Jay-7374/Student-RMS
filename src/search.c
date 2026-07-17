/**
 * @file    search.c
 * @brief   Search algorithm implementations.
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../server/search.h"
#include "../common/validation.h"

/* =========================================================================
 * SearchResult memory management
 * ========================================================================= */

#define INITIAL_CAPACITY 16

static ResultCode result_append(SearchResult *r, int idx)
{
    if (r->count >= r->capacity) {
        int new_cap = (r->capacity == 0) ? INITIAL_CAPACITY : r->capacity * 2;
        int *new_arr = (int *)realloc(r->indices, (size_t)new_cap * sizeof(int));
        if (!new_arr) return RESULT_ERR_MEMORY;
        r->indices  = new_arr;
        r->capacity = new_cap;
    }
    r->indices[r->count++] = idx;
    return RESULT_OK;
}

void search_result_free(SearchResult *result)
{
    if (!result) return;
    free(result->indices);
    result->indices  = NULL;
    result->count    = 0;
    result->capacity = 0;
}

void search_result_print(const StudentRegistry *reg, const SearchResult *result)
{
    int i;
    if (!reg || !result || result->count == 0) {
        printf("  No results.\n");
        return;
    }
    printf("%-4s %-8s %-22s %-18s %-5s %-6s\n",
           "#", "ID", "Name", "Department", "Year", "GPA");
    for (i = 0; i < result->count; i++) {
        const Student *s = &reg->records[result->indices[i]];
        printf("%-4d %-8d %-22s %-18s %-5d %.2f\n",
               i + 1, s->student_id, s->name, s->department,
               s->year_of_study, s->gpa);
    }
}

/* =========================================================================
 * Single-result — binary search by ID (requires sorted registry)
 * ========================================================================= */

Student *search_by_id(StudentRegistry *reg, int student_id)
{
    int lo, hi, mid;
    if (!reg || reg->count == 0) return NULL;

    lo = 0;
    hi = MAX_STUDENTS - 1;

    while (lo <= hi) {
        mid = lo + (hi - lo) / 2;

        /* Skip inactive slots — fall back to linear scan for safety */
        if (!reg->records[mid].is_active) {
            /* Try a linear scan as fallback to handle gaps */
            int i;
            for (i = 0; i < MAX_STUDENTS; i++) {
                if (reg->records[i].is_active &&
                    reg->records[i].student_id == student_id) {
                    return &reg->records[i];
                }
            }
            return NULL;
        }

        if (reg->records[mid].student_id == student_id) {
            return &reg->records[mid];
        } else if (reg->records[mid].student_id < student_id) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return NULL;
}

/* =========================================================================
 * Single-result — exact name match (linear)
 * ========================================================================= */

Student *search_by_name_exact(StudentRegistry *reg, const char *name)
{
    int  i;
    char n1[MAX_NAME_LEN], n2[MAX_NAME_LEN];

    if (!reg || !name) return NULL;

    strncpy(n1, name, sizeof(n1) - 1);
    n1[sizeof(n1) - 1] = '\0';
    val_to_lowercase(n1);

    for (i = 0; i < MAX_STUDENTS; i++) {
        if (!reg->records[i].is_active) continue;
        strncpy(n2, reg->records[i].name, sizeof(n2) - 1);
        n2[sizeof(n2) - 1] = '\0';
        val_to_lowercase(n2);
        if (strcmp(n1, n2) == 0) return &reg->records[i];
    }
    return NULL;
}

/* =========================================================================
 * Case-insensitive helpers (Windows / MinGW compat)
 * =========================================================================
 * MinGW declares _strnicmp in <string.h> as a non-static symbol, so we
 * cannot declare our own 'strncasecmp' as static without a conflict.
 * We use a private name 'local_strncasecmp' throughout this file instead.
 */
#ifdef _WIN32
static int local_strncasecmp(const char *s1, const char *s2, size_t n)
{
    while (n-- > 0) {
        int d = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
        if (d != 0 || *s1 == '\0') return d;
        s1++; s2++;
    }
    return 0;
}
#else
/* On POSIX systems strncasecmp is provided by <strings.h> */
#  include <strings.h>
#  define local_strncasecmp strncasecmp
#endif

/* =========================================================================
 * Multi-result — substring name search
 * ========================================================================= */

/* Case-insensitive strstr */
static const char *stristr(const char *haystack, const char *needle)
{
    size_t nlen;
    if (!haystack || !needle || *needle == '\0') return haystack;
    nlen = strlen(needle);

    for (; *haystack; haystack++) {
        if (local_strncasecmp(haystack, needle, nlen) == 0) return haystack;
    }
    return NULL;
}



ResultCode search_by_name(const StudentRegistry *reg, const char *keyword,
                           SearchResult *out)
{
    int i;
    if (!reg || !keyword || !out) return RESULT_ERR_INVALID;

    memset(out, 0, sizeof(SearchResult));

    for (i = 0; i < MAX_STUDENTS; i++) {
        if (!reg->records[i].is_active) continue;
        if (stristr(reg->records[i].name, keyword)) {
            ResultCode rc = result_append(out, i);
            if (rc != RESULT_OK) {
                search_result_free(out);
                return rc;
            }
        }
    }
    return RESULT_OK;
}

/* =========================================================================
 * Multi-result — department exact match
 * ========================================================================= */

ResultCode search_by_department(const StudentRegistry *reg, const char *dept,
                                 SearchResult *out)
{
    int  i;
    char d1[MAX_DEPT_LEN], d2[MAX_DEPT_LEN];

    if (!reg || !dept || !out) return RESULT_ERR_INVALID;

    memset(out, 0, sizeof(SearchResult));
    strncpy(d1, dept, sizeof(d1) - 1);
    d1[sizeof(d1) - 1] = '\0';
    val_to_lowercase(d1);

    for (i = 0; i < MAX_STUDENTS; i++) {
        if (!reg->records[i].is_active) continue;
        strncpy(d2, reg->records[i].department, sizeof(d2) - 1);
        d2[sizeof(d2) - 1] = '\0';
        val_to_lowercase(d2);
        if (strcmp(d1, d2) == 0) {
            ResultCode rc = result_append(out, i);
            if (rc != RESULT_OK) { search_result_free(out); return rc; }
        }
    }
    return RESULT_OK;
}

/* =========================================================================
 * Multi-result — GPA range
 * ========================================================================= */

ResultCode search_by_gpa_range(const StudentRegistry *reg, float min_gpa,
                                float max_gpa, SearchResult *out)
{
    int i;
    if (!reg || !out) return RESULT_ERR_INVALID;

    memset(out, 0, sizeof(SearchResult));

    for (i = 0; i < MAX_STUDENTS; i++) {
        if (!reg->records[i].is_active) continue;
        if (reg->records[i].gpa >= min_gpa && reg->records[i].gpa <= max_gpa) {
            ResultCode rc = result_append(out, i);
            if (rc != RESULT_OK) { search_result_free(out); return rc; }
        }
    }
    return RESULT_OK;
}

/* =========================================================================
 * Multi-result — year of study
 * ========================================================================= */

ResultCode search_by_year(const StudentRegistry *reg, int year,
                           SearchResult *out)
{
    int i;
    if (!reg || !out) return RESULT_ERR_INVALID;

    memset(out, 0, sizeof(SearchResult));

    for (i = 0; i < MAX_STUDENTS; i++) {
        if (!reg->records[i].is_active) continue;
        if (reg->records[i].year_of_study == year) {
            ResultCode rc = result_append(out, i);
            if (rc != RESULT_OK) { search_result_free(out); return rc; }
        }
    }
    return RESULT_OK;
}

/* =========================================================================
 * Interactive dispatcher (server-side; not used in client build)
 * ========================================================================= */

void search_interactive(StudentRegistry *reg)
{
    (void)reg; /* No-op stub; search is driven via protocol on client side */
}
