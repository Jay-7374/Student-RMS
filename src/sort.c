/**
 * @file    sort.c
 * @brief   Sorting algorithm implementations.
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../server/sort.h"
#include "../common/student.h"

/* =========================================================================
 * Helper — case-insensitive string compare
 * ========================================================================= */

#ifdef _WIN32
    #include <string.h>
    #define STRCASECMP _stricmp
#else
    #include <strings.h>
    #define STRCASECMP strcasecmp
#endif

/* =========================================================================
 * Tier 2 — qsort()-compatible comparators
 * ========================================================================= */

int cmp_by_name(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    int r = STRCASECMP(sa->name, sb->name);
    if (r != 0) return r;
    /* Tie-break by student_id ascending */
    return sa->student_id - sb->student_id;
}

int cmp_by_gpa(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    if (sa->gpa < sb->gpa) return -1;
    if (sa->gpa > sb->gpa) return  1;
    /* Tie-break by name */
    return STRCASECMP(sa->name, sb->name);
}

int cmp_by_gpa_desc(const void *a, const void *b)
{
    return cmp_by_gpa(b, a);   /* reverse arguments */
}

int cmp_by_roll(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    return sa->roll_number - sb->roll_number;
}

int cmp_by_id(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    return sa->student_id - sb->student_id;
}

int cmp_by_dept_then_name(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    int r = STRCASECMP(sa->department, sb->department);
    if (r != 0) return r;
    return STRCASECMP(sa->name, sb->name);
}

/* =========================================================================
 * Internal: reverse-wrap any ascending comparator for descending sort
 * ========================================================================= */

typedef int (*CmpFn)(const void *, const void *);

/* We use a global pointer trick for descending sort via qsort wrapper.
 * Single-threaded server — no concurrency concern. */
static CmpFn g_cmp_fn = NULL;

static int cmp_reversed(const void *a, const void *b)
{
    return g_cmp_fn(b, a);
}

/* =========================================================================
 * Tier 1 — High-level registry sort API
 * ========================================================================= */

void sort_by_name(StudentRegistry *reg, SortOrder order)
{
    if (!reg || reg->count == 0) return;

    if (order == SORT_ASCENDING) {
        qsort(reg->records, MAX_STUDENTS, sizeof(Student), cmp_by_name);
    } else {
        g_cmp_fn = cmp_by_name;
        qsort(reg->records, MAX_STUDENTS, sizeof(Student), cmp_reversed);
    }
}

void sort_by_gpa(StudentRegistry *reg, SortOrder order)
{
    if (!reg || reg->count == 0) return;

    if (order == SORT_DESCENDING) {
        qsort(reg->records, MAX_STUDENTS, sizeof(Student), cmp_by_gpa_desc);
    } else {
        qsort(reg->records, MAX_STUDENTS, sizeof(Student), cmp_by_gpa);
    }
}

void sort_by_roll(StudentRegistry *reg, SortOrder order)
{
    if (!reg || reg->count == 0) return;

    if (order == SORT_ASCENDING) {
        qsort(reg->records, MAX_STUDENTS, sizeof(Student), cmp_by_roll);
    } else {
        g_cmp_fn = cmp_by_roll;
        qsort(reg->records, MAX_STUDENTS, sizeof(Student), cmp_reversed);
    }
}

void sort_by_id(StudentRegistry *reg)
{
    if (!reg) return;
    qsort(reg->records, MAX_STUDENTS, sizeof(Student), cmp_by_id);
}

void sort_by_department(StudentRegistry *reg, SortOrder order)
{
    if (!reg || reg->count == 0) return;

    if (order == SORT_ASCENDING) {
        qsort(reg->records, MAX_STUDENTS, sizeof(Student), cmp_by_dept_then_name);
    } else {
        g_cmp_fn = cmp_by_dept_then_name;
        qsort(reg->records, MAX_STUDENTS, sizeof(Student), cmp_reversed);
    }
}

/* =========================================================================
 * Tier 3 — Insertion sort (educational, O(n²), stable)
 *
 * Complexity:
 *   Best case:    O(n)   — already sorted
 *   Average case: O(n²)
 *   Worst case:   O(n²)  — reverse sorted
 *   Space:        O(1)   — in-place, no extra allocation
 * ========================================================================= */

void sort_insertion_by_name(StudentRegistry *reg)
{
    int     i, j;
    Student key;

    if (!reg) return;

    for (i = 1; i < MAX_STUDENTS; i++) {
        key = reg->records[i];
        j   = i - 1;

        while (j >= 0 &&
               STRCASECMP(reg->records[j].name, key.name) > 0) {
            reg->records[j + 1] = reg->records[j];
            j--;
        }
        reg->records[j + 1] = key;
    }
}

void sort_insertion_by_gpa_desc(StudentRegistry *reg)
{
    int     i, j;
    Student key;

    if (!reg) return;

    for (i = 1; i < MAX_STUDENTS; i++) {
        key = reg->records[i];
        j   = i - 1;

        /* Descending: move elements with LOWER gpa to the right */
        while (j >= 0 && reg->records[j].gpa < key.gpa) {
            reg->records[j + 1] = reg->records[j];
            j--;
        }
        reg->records[j + 1] = key;
    }
}

/* =========================================================================
 * Interactive dispatcher stub (no I/O in server build)
 * ========================================================================= */

void sort_interactive(StudentRegistry *reg)
{
    (void)reg; /* Sorting is driven by Request.sort_key on client side */
}
