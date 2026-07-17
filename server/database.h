/**
 * @file    database.h
 * @brief   Database abstraction layer — the ONLY interface the server uses
 *          to access student data.
 *
 * Responsibilities:
 *   - Own the single in-memory StudentRegistry for the server process.
 *   - Wrap student.c CRUD functions and file_manager.c I/O behind a clean
 *     db_* API so that request_handler.c has zero direct knowledge of
 *     file paths, registry internals, or search/sort algorithms.
 *   - Provide atomic persistence: every mutating db_* call auto-saves.
 *
 * Threading model (Phase 1 — single-threaded):
 *   All db_* functions are called from a single thread. The function
 *   signatures include a mutex slot comment for Phase 2 multithreading:
 *   wrap the body in pthread_mutex_lock(&db_mutex) / unlock.
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#ifndef DATABASE_H
#define DATABASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../common/student.h"   /* Student, StudentRegistry, ResultCode */

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

/**
 * @brief  Initialise the database layer.
 *
 * Ensures the data/ directory exists, loads students.dat into the internal
 * registry, and sets up any state needed for subsequent db_* calls.
 * Must be called once before any other db_* function.
 *
 * @return RESULT_OK            on success (including first-run empty file).
 * @return RESULT_ERR_CORRUPT   if students.dat has invalid magic/checksum.
 * @return RESULT_ERR_PERMISSION if data directory cannot be created.
 */
ResultCode db_init(void);

/**
 * @brief  Flush the registry to disk and release any held resources.
 *
 * Performs an atomic save (write-to-tmp then rename). Should be called
 * before the server process exits.
 *
 * @return RESULT_OK on success.
 * @return RESULT_ERR_FILE on save failure.
 */
ResultCode db_shutdown(void);

/**
 * @brief  Force an immediate flush of the registry to students.dat.
 *
 * Normally not needed (db_add/update/delete auto-save), but exposed
 * for use after bulk operations or administrative requests.
 *
 * @return RESULT_OK on success, RESULT_ERR_FILE on failure.
 */
ResultCode db_flush(void);

/* =========================================================================
 * CRUD operations
 * ========================================================================= */

/**
 * @brief  Add a new student record.
 *
 * Validates, checks for duplicate ID, appends to registry, saves to disk.
 *
 * @param  s  Fully populated Student struct. Must not be NULL.
 * @return RESULT_OK            on success.
 * @return RESULT_ERR_DUPLICATE if student_id already exists.
 * @return RESULT_ERR_FULL      if registry is at MAX_STUDENTS capacity.
 * @return RESULT_ERR_INVALID   if any field fails validation.
 * @return RESULT_ERR_FILE      if auto-save fails.
 */
ResultCode db_add_student(const Student *s);

/**
 * @brief  Retrieve all active student records.
 *
 * Copies active records into the caller-supplied array. The array must
 * be large enough to hold MAX_STUDENTS entries; in practice count <= 500.
 *
 * @param  out_arr    Caller-allocated array of Student (size >= MAX_STUDENTS).
 * @param  out_count  Receives the number of active records copied.
 * @param  sort_key   SortKey value (0 = unsorted / storage order).
 * @return RESULT_OK always (count == 0 means no active records).
 */
ResultCode db_get_all(Student *out_arr, int *out_count, int sort_key);

/**
 * @brief  Retrieve a single active student by primary key.
 *
 * @param  id   Student ID to look up.
 * @param  out  Output buffer populated on RESULT_OK. Must not be NULL.
 * @return RESULT_OK           on success.
 * @return RESULT_ERR_NOT_FOUND if no active record matches.
 */
ResultCode db_get_by_id(int id, Student *out);

/**
 * @brief  Update an existing student record.
 *
 * The student_id in @p s identifies the record. All other fields are
 * replaced. Validates all fields before committing.
 *
 * @param  s  Student struct with updated fields. Must not be NULL.
 * @return RESULT_OK           on success.
 * @return RESULT_ERR_NOT_FOUND if no active record with that ID exists.
 * @return RESULT_ERR_INVALID   if any field fails validation.
 * @return RESULT_ERR_FILE      if auto-save fails.
 */
ResultCode db_update_student(const Student *s);

/**
 * @brief  Soft-delete a student record by primary key.
 *
 * Sets is_active = 0 on the matching record and saves.
 *
 * @param  id  Student ID to delete.
 * @return RESULT_OK           on success.
 * @return RESULT_ERR_NOT_FOUND if no active record matches.
 * @return RESULT_ERR_FILE      if auto-save fails.
 */
ResultCode db_delete_student(int id);

/* =========================================================================
 * Search operations
 * ========================================================================= */

/**
 * @brief  Search active records by name substring (case-insensitive).
 *
 * @param  keyword    Search term. Must not be NULL.
 * @param  out_arr    Caller array for results (size >= MAX_STUDENTS).
 * @param  out_count  Receives the number of matches.
 * @return RESULT_OK always (count == 0 means no matches).
 */
ResultCode db_search_by_name(const char *keyword, Student *out_arr, int *out_count);

/**
 * @brief  Search active records by exact department (case-insensitive).
 *
 * @param  dept       Department name to match. Must not be NULL.
 * @param  out_arr    Caller array for results.
 * @param  out_count  Receives the number of matches.
 * @return RESULT_OK always.
 */
ResultCode db_search_by_dept(const char *dept, Student *out_arr, int *out_count);

/* =========================================================================
 * Report / statistics
 * ========================================================================= */

/**
 * @brief  Compute aggregate statistics over all active records.
 *
 * @param  avg_gpa      Receives average GPA (0.0 if no active records).
 * @param  top_gpa      Receives highest GPA (0.0 if no active records).
 * @param  total        Receives total active record count.
 * @param  top_student  Receives a copy of the top-GPA student. May be NULL.
 * @return RESULT_OK always.
 */
ResultCode db_report(float *avg_gpa, float *top_gpa, int *total,
                     Student *top_student);

#ifdef __cplusplus
}
#endif

#endif /* DATABASE_H */
