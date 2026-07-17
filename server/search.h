/**
 * @file    search.h
 * @brief   Search algorithms for the Student Record System.
 *
 * This module provides two categories of search:
 *
 *   1. Single-result searches  — return a pointer into the registry's
 *      internal array (or NULL). O(log n) via binary search (by ID) or
 *      O(n) via linear scan (by name, department).
 *
 *   2. Multi-result searches   — return a dynamically allocated array of
 *      matching indices (SearchResult). The caller is responsible for
 *      calling search_result_free() when done.
 *
 * IMPORTANT: Pointers returned by single-result functions are valid only
 * while the registry has not been mutated (add/update/delete/sort). Treat
 * them as short-lived views, not stable references.
 *
 * Binary search pre-condition:
 *   search_by_id() requires the registry to be sorted by student_id
 *   ascending. Call sort_by_id() before the first search if data was
 *   loaded from file or if records were added since the last sort.
 *   The flag reg->is_sorted_by_id (tracked internally) drives this.
 *
 * @author  Student Record System
 * @version 1.0.0
 */

#ifndef SEARCH_H
#define SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../common/student.h"   /* StudentRegistry, Student, ResultCode */

/* =========================================================================
 * Search result set (multi-match)
 * ========================================================================= */

/**
 * @brief  A dynamically allocated collection of matched record indices.
 *
 * Indices refer to positions in StudentRegistry.records[].
 * The count field holds the number of valid entries in indices[].
 *
 * Memory: allocated by search functions; freed by search_result_free().
 */
typedef struct {
    int    *indices;  /**< Heap-allocated array of registry indices.     */
    int     count;    /**< Number of valid entries in indices[].          */
    int     capacity; /**< Allocated capacity (internal; do not use).    */
} SearchResult;

/**
 * @brief  Free the heap memory owned by a SearchResult.
 *
 * Sets indices to NULL and count/capacity to 0. Safe to call on a
 * zeroed/uninitialised SearchResult.
 *
 * @param  result  Pointer to the SearchResult to free. Must not be NULL.
 */
void search_result_free(SearchResult *result);

/**
 * @brief  Print all students referenced by a SearchResult as a table.
 *
 * @param  reg     Registry containing the referenced records. Must not be NULL.
 * @param  result  Result set to display. Must not be NULL.
 */
void search_result_print(const StudentRegistry *reg, const SearchResult *result);

/* =========================================================================
 * Single-result searches (return pointer into registry)
 * ========================================================================= */

/**
 * @brief  Find an active student by exact student_id using binary search.
 *
 * Pre-condition: The registry must be sorted by student_id ascending.
 * If the registry has been modified since the last sort, call
 * sort_by_id() first.
 *
 * Complexity: O(log n) average and worst case.
 *
 * @param  reg         Registry to search. Must not be NULL.
 * @param  student_id  Target primary key.
 * @return Pointer to the matching Student within the registry, or NULL if
 *         not found or the record is soft-deleted.
 */
Student *search_by_id(StudentRegistry *reg, int student_id);

/**
 * @brief  Find the first active student whose name matches exactly (case-
 *         insensitive) using linear search.
 *
 * Complexity: O(n).
 *
 * @param  reg   Registry to search. Must not be NULL.
 * @param  name  Exact name to match (case-insensitive). Must not be NULL.
 * @return Pointer to the first matching Student, or NULL if not found.
 */
Student *search_by_name_exact(StudentRegistry *reg, const char *name);

/* =========================================================================
 * Multi-result searches (return SearchResult)
 * ========================================================================= */

/**
 * @brief  Find all active students whose name contains a given substring
 *         (case-insensitive).
 *
 * Complexity: O(n * k) where k is the length of the keyword.
 *
 * @param  reg      Registry to search. Must not be NULL.
 * @param  keyword  Substring to search for. Must not be NULL.
 * @param  out      Output SearchResult. Caller must free with search_result_free().
 * @return RESULT_OK            on success (even if count == 0).
 * @return RESULT_ERR_MEMORY    if heap allocation for the result set fails.
 */
ResultCode search_by_name(const StudentRegistry *reg, const char *keyword,
                           SearchResult *out);

/**
 * @brief  Find all active students belonging to a given department
 *         (case-insensitive, exact match).
 *
 * Complexity: O(n).
 *
 * @param  reg   Registry to search. Must not be NULL.
 * @param  dept  Department name to match. Must not be NULL.
 * @param  out   Output SearchResult. Caller must free with search_result_free().
 * @return RESULT_OK            on success (even if count == 0).
 * @return RESULT_ERR_MEMORY    if heap allocation fails.
 */
ResultCode search_by_department(const StudentRegistry *reg, const char *dept,
                                 SearchResult *out);

/**
 * @brief  Find all active students whose GPA is within [min_gpa, max_gpa].
 *
 * Useful for report filtering (e.g., "students with GPA >= 8.0").
 * Complexity: O(n).
 *
 * @param  reg      Registry to search. Must not be NULL.
 * @param  min_gpa  Lower GPA bound (inclusive).
 * @param  max_gpa  Upper GPA bound (inclusive).
 * @param  out      Output SearchResult. Caller must free with search_result_free().
 * @return RESULT_OK            on success (even if count == 0).
 * @return RESULT_ERR_MEMORY    if heap allocation fails.
 */
ResultCode search_by_gpa_range(const StudentRegistry *reg, float min_gpa,
                                float max_gpa, SearchResult *out);

/**
 * @brief  Find all active students in a given year of study.
 *
 * Complexity: O(n).
 *
 * @param  reg   Registry to search. Must not be NULL.
 * @param  year  Year of study to match [YEAR_MIN, YEAR_MAX].
 * @param  out   Output SearchResult. Caller must free with search_result_free().
 * @return RESULT_OK            on success (even if count == 0).
 * @return RESULT_ERR_MEMORY    if heap allocation fails.
 */
ResultCode search_by_year(const StudentRegistry *reg, int year,
                           SearchResult *out);

/* =========================================================================
 * Interactive search dispatcher (called from main.c menu)
 * ========================================================================= */

/**
 * @brief  Display a search sub-menu, collect criteria, run the appropriate
 *         search, and print results.
 *
 * This is the only function in search.h that performs I/O. It is isolated
 * here so that the other search functions remain unit-testable without a
 * terminal.
 *
 * @param  reg  Registry to search. Must not be NULL.
 */
void search_interactive(StudentRegistry *reg);

#ifdef __cplusplus
}
#endif

#endif /* SEARCH_H */
