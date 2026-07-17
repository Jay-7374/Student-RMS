/**
 * @file    student.c
 * @brief   StudentRegistry CRUD operations and display helpers.
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../common/student.h"
#include "../common/validation.h"

/* =========================================================================
 * ResultCode → string
 * ========================================================================= */

const char *result_to_string(ResultCode code)
{
    switch (code) {
        case RESULT_OK:             return "OK";
        case RESULT_ERR_NOT_FOUND:  return "Record not found";
        case RESULT_ERR_DUPLICATE:  return "Duplicate student ID";
        case RESULT_ERR_FULL:       return "Registry is full (max 500)";
        case RESULT_ERR_INVALID:    return "Invalid input";
        case RESULT_ERR_FILE:       return "File I/O error";
        case RESULT_ERR_CORRUPT:    return "File corrupted or invalid format";
        case RESULT_ERR_PERMISSION: return "Permission denied";
        case RESULT_ERR_AUTH:       return "Authentication failed";
        case RESULT_ERR_MEMORY:     return "Memory allocation failed";
        default:                    return "Unknown error";
    }
}

/* =========================================================================
 * Registry lifecycle
 * ========================================================================= */

void registry_init(StudentRegistry *reg)
{
    if (!reg) return;
    memset(reg, 0, sizeof(StudentRegistry));
    reg->next_id    = 1;
    reg->count      = 0;
    reg->total_ever = 0;
    reg->is_dirty   = 0;
}

void registry_destroy(StudentRegistry *reg)
{
    /* No-op for fixed-array implementation.
     * Present for forward-compatibility with dynamic allocation. */
    (void)reg;
}

/* =========================================================================
 * Internal helpers
 * ========================================================================= */

/** Find the index of an ACTIVE record with the given student_id.
 *  Returns -1 if not found or is_active == 0. */
static int find_active_index(const StudentRegistry *reg, int student_id)
{
    int i;
    for (i = 0; i < MAX_STUDENTS; i++) {
        if (reg->records[i].is_active &&
            reg->records[i].student_id == student_id) {
            return i;
        }
    }
    return -1;
}

/** Find the first empty slot (is_active == 0) in the records array.
 *  Returns -1 if registry is full. */
static int find_empty_slot(const StudentRegistry *reg)
{
    int i;
    for (i = 0; i < MAX_STUDENTS; i++) {
        if (!reg->records[i].is_active) return i;
    }
    return -1;
}

/* =========================================================================
 * CRUD operations
 * ========================================================================= */

ResultCode student_add(StudentRegistry *reg, const Student *student)
{
    int slot;

    if (!reg || !student) return RESULT_ERR_INVALID;

    /* Capacity check */
    if (reg->count >= MAX_STUDENTS) return RESULT_ERR_FULL;

    /* Duplicate ID check */
    if (find_active_index(reg, student->student_id) >= 0)
        return RESULT_ERR_DUPLICATE;

    /* Basic field validation */
    if (!val_is_valid_student_id(student->student_id)) return RESULT_ERR_INVALID;
    if (!val_is_valid_name(student->name))              return RESULT_ERR_INVALID;
    if (!val_is_valid_gpa(student->gpa))                return RESULT_ERR_INVALID;
    if (!val_is_valid_department(student->department))  return RESULT_ERR_INVALID;

    slot = find_empty_slot(reg);
    if (slot < 0) return RESULT_ERR_FULL;

    reg->records[slot]              = *student;
    reg->records[slot].is_active    = 1;
    reg->records[slot].enrollment_date = (student->enrollment_date != 0)
                                          ? student->enrollment_date
                                          : time(NULL);

    reg->count++;
    reg->total_ever++;

    /* Advance next_id if this student used an ID >= current next_id */
    if (student->student_id >= reg->next_id) {
        reg->next_id = student->student_id + 1;
    }

    reg->is_dirty = 1;
    return RESULT_OK;
}

ResultCode student_get_by_id(const StudentRegistry *reg, int student_id,
                              Student *out)
{
    int idx;
    if (!reg || !out) return RESULT_ERR_INVALID;

    idx = find_active_index(reg, student_id);
    if (idx < 0) return RESULT_ERR_NOT_FOUND;

    *out = reg->records[idx];
    return RESULT_OK;
}

ResultCode student_update(StudentRegistry *reg, const Student *updated)
{
    int idx;
    if (!reg || !updated) return RESULT_ERR_INVALID;

    /* Validate updated fields */
    if (!val_is_valid_name(updated->name))             return RESULT_ERR_INVALID;
    if (!val_is_valid_gpa(updated->gpa))               return RESULT_ERR_INVALID;
    if (!val_is_valid_department(updated->department)) return RESULT_ERR_INVALID;

    idx = find_active_index(reg, updated->student_id);
    if (idx < 0) return RESULT_ERR_NOT_FOUND;

    /* Preserve enrollment_date and is_active from original record */
    {
        time_t  original_date   = reg->records[idx].enrollment_date;
        reg->records[idx]       = *updated;
        reg->records[idx].is_active      = 1;
        reg->records[idx].enrollment_date = original_date;
    }

    reg->is_dirty = 1;
    return RESULT_OK;
}

ResultCode student_delete(StudentRegistry *reg, int student_id)
{
    int idx;
    if (!reg) return RESULT_ERR_INVALID;

    idx = find_active_index(reg, student_id);
    if (idx < 0) return RESULT_ERR_NOT_FOUND;

    reg->records[idx].is_active = 0;
    reg->count--;
    reg->is_dirty = 1;
    return RESULT_OK;
}

int student_purge_deleted(StudentRegistry *reg)
{
    int src, dst = 0, purged = 0;
    if (!reg) return 0;

    for (src = 0; src < MAX_STUDENTS; src++) {
        if (reg->records[src].is_active) {
            if (src != dst) {
                reg->records[dst] = reg->records[src];
                memset(&reg->records[src], 0, sizeof(Student));
            }
            dst++;
        } else if (reg->records[src].student_id != 0) {
            memset(&reg->records[src], 0, sizeof(Student));
            purged++;
        }
    }

    reg->is_dirty = (purged > 0) ? 1 : reg->is_dirty;
    return purged;
}

/* =========================================================================
 * Statistics
 * ========================================================================= */

ResultCode student_get_top_gpa(const StudentRegistry *reg, Student *out)
{
    int   i;
    int   found = 0;
    float best  = -1.0f;
    int   best_idx = -1;

    if (!reg || !out) return RESULT_ERR_INVALID;

    for (i = 0; i < MAX_STUDENTS; i++) {
        if (reg->records[i].is_active && reg->records[i].gpa > best) {
            best     = reg->records[i].gpa;
            best_idx = i;
            found    = 1;
        }
    }

    if (!found) return RESULT_ERR_NOT_FOUND;
    *out = reg->records[best_idx];
    return RESULT_OK;
}

ResultCode student_average_gpa(const StudentRegistry *reg, float *out)
{
    int   i;
    float total = 0.0f;
    int   count = 0;

    if (!reg || !out) return RESULT_ERR_INVALID;

    for (i = 0; i < MAX_STUDENTS; i++) {
        if (reg->records[i].is_active) {
            total += reg->records[i].gpa;
            count++;
        }
    }

    if (count == 0) return RESULT_ERR_NOT_FOUND;
    *out = total / (float)count;
    return RESULT_OK;
}

int student_count_by_department(const StudentRegistry *reg, const char *dept)
{
    int   i, count = 0;
    char  dept_lower[MAX_DEPT_LEN];
    char  rec_lower[MAX_DEPT_LEN];

    if (!reg || !dept) return 0;

    strncpy(dept_lower, dept, sizeof(dept_lower) - 1);
    dept_lower[sizeof(dept_lower) - 1] = '\0';
    val_to_lowercase(dept_lower);

    for (i = 0; i < MAX_STUDENTS; i++) {
        if (!reg->records[i].is_active) continue;
        strncpy(rec_lower, reg->records[i].department, sizeof(rec_lower) - 1);
        rec_lower[sizeof(rec_lower) - 1] = '\0';
        val_to_lowercase(rec_lower);
        if (strcmp(rec_lower, dept_lower) == 0) count++;
    }
    return count;
}

/* =========================================================================
 * Display helpers
 * ========================================================================= */

void student_print_table_header(void)
{
    printf("%-4s %-8s %-22s %-18s %-5s %-6s\n",
           "#", "ID", "Name", "Department", "Year", "GPA");
    printf("──── ──────── ────────────────────── "
           "────────────────── ───── ──────\n");
}

void student_print_row(const Student *s, int index)
{
    if (!s) return;
    printf("%-4d %-8d %-22s %-18s %-5d %.2f\n",
           index, s->student_id, s->name, s->department,
           s->year_of_study, s->gpa);
}

void student_print_detail(const Student *s)
{
    char buf[32] = {0};
    struct tm *tm_info;

    if (!s) return;
    tm_info = localtime(&s->enrollment_date);
    if (tm_info) strftime(buf, sizeof(buf), "%Y-%m-%d", tm_info);

    printf("  ID          : %d\n",   s->student_id);
    printf("  Name        : %s\n",   s->name);
    printf("  Roll        : %d\n",   s->roll_number);
    printf("  Department  : %s\n",   s->department);
    printf("  Year        : %d\n",   s->year_of_study);
    printf("  Age         : %d\n",   s->age);
    printf("  GPA         : %.2f\n", s->gpa);
    printf("  Email       : %s\n",   s->email);
    printf("  Phone       : %s\n",   s->phone);
    printf("  Address     : %s\n",   s->address);
    printf("  Enrolled    : %s\n",   buf);
}

void student_list_all(const StudentRegistry *reg, int page_size)
{
    int i, shown = 0;
    if (!reg) return;

    student_print_table_header();
    for (i = 0; i < MAX_STUDENTS; i++) {
        if (!reg->records[i].is_active) continue;
        student_print_row(&reg->records[i], ++shown);
        if (page_size > 0 && shown % page_size == 0) {
            printf("  -- Press Enter for next page --");
            getchar();
        }
    }
    printf("\n  Total: %d active record(s)\n", shown);
}

/* Prompt functions remain server-local; stubs satisfy the header contract */
void student_prompt_new(Student *out, const StudentRegistry *reg)
{
    (void)out; (void)reg; /* Implemented in client_menu.c for the client side */
}

void student_prompt_edit(Student *s)
{
    (void)s; /* Implemented in client_menu.c for the client side */
}
