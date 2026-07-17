/**
 * @file    sort.h
 * @brief   Sorting algorithms and comparators for the Student Record System.
 *
 * This module exposes two tiers of sorting:
 *
 *   Tier 1 — High-level registry sort functions (sort_by_*):
 *     Each function sorts reg->records[] in-place using the C standard
 *     library qsort(). These are the primary API for main.c and search.c.
 *
 *   Tier 2 — Raw comparator functions (cmp_*):
 *     qsort()-compatible comparison functions suitable for direct use with
 *     qsort(), bsearch(), or any algorithm that accepts a comparator.
 *     These functions accept (const void *, const void *) pointers to
 *     Student structs and return negative/zero/positive integers.
 *
 *   Tier 3 — Educational alternative (sort_insertion_*):
 *     Manual O(n²) insertion sort implementations included to demonstrate
 *     algorithmic awareness. Documented with complexity analysis.
 *
 * Sort stability:
 *   C's qsort() is not guaranteed to be stable. For stable sorts (e.g.,
 *   sort by GPA, then by name for ties), use the composite comparators
 *   whose names end in _stable.
 *
 * @author  Student Record System
 * @version 1.0.0
 */

#ifndef SORT_H
#define SORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../common/student.h"   /* StudentRegistry, Student */

/* =========================================================================
 * Sort order enumeration
 * ========================================================================= */

/**
 * @brief  Sort direction used by high-level sort functions.
 */
typedef enum {
    SORT_ASCENDING  = 0, /**< Smallest to largest (A→Z, 0→9).  */
    SORT_DESCENDING = 1  /**< Largest to smallest (Z→A, 9→0).  */
} SortOrder;

/* =========================================================================
 * Tier 1 — High-level registry sort API
 * ========================================================================= */

/**
 * @brief  Sort the registry by student name (alphabetical).
 *
 * Uses qsort() with cmp_by_name. Ties (identical names) are broken by
 * student_id ascending to ensure deterministic output.
 *
 * Complexity: O(n log n) average.
 *
 * @param  reg    Registry to sort in-place. Must not be NULL.
 * @param  order  SORT_ASCENDING (A→Z) or SORT_DESCENDING (Z→A).
 */
void sort_by_name(StudentRegistry *reg, SortOrder order);

/**
 * @brief  Sort the registry by GPA.
 *
 * Uses qsort() with cmp_by_gpa. Ties broken by name ascending.
 * Default display order for rankings: SORT_DESCENDING (highest GPA first).
 *
 * Complexity: O(n log n) average.
 *
 * @param  reg    Registry to sort in-place. Must not be NULL.
 * @param  order  SORT_ASCENDING or SORT_DESCENDING.
 */
void sort_by_gpa(StudentRegistry *reg, SortOrder order);

/**
 * @brief  Sort the registry by roll number (ascending only).
 *
 * Roll numbers are assumed unique within a batch; no tie-breaking needed.
 * Complexity: O(n log n) average.
 *
 * @param  reg    Registry to sort in-place. Must not be NULL.
 * @param  order  SORT_ASCENDING or SORT_DESCENDING.
 */
void sort_by_roll(StudentRegistry *reg, SortOrder order);

/**
 * @brief  Sort the registry by student_id ascending.
 *
 * This is a pre-requisite for binary search (search_by_id()).
 * Complexity: O(n log n) average.
 *
 * @param  reg  Registry to sort in-place. Must not be NULL.
 */
void sort_by_id(StudentRegistry *reg);

/**
 * @brief  Sort the registry by department name, then by name within each
 *         department.
 *
 * Useful for department-wise report generation.
 * Complexity: O(n log n) average.
 *
 * @param  reg    Registry to sort in-place. Must not be NULL.
 * @param  order  SORT_ASCENDING or SORT_DESCENDING (applies to dept name).
 */
void sort_by_department(StudentRegistry *reg, SortOrder order);

/* =========================================================================
 * Tier 2 — qsort()-compatible comparators (raw)
 * ========================================================================= */

/**
 * @brief  Compare two Student structs by name (case-insensitive, ascending).
 *
 * @param  a  Pointer to first Student. Must not be NULL.
 * @param  b  Pointer to second Student. Must not be NULL.
 * @return Negative if a < b, 0 if equal, positive if a > b.
 */
int cmp_by_name(const void *a, const void *b);

/**
 * @brief  Compare two Student structs by GPA (ascending).
 * @param  a  Pointer to first Student.
 * @param  b  Pointer to second Student.
 * @return Negative if a.gpa < b.gpa, 0 if equal, positive if a.gpa > b.gpa.
 */
int cmp_by_gpa(const void *a, const void *b);

/**
 * @brief  Compare two Student structs by GPA (descending).
 *
 * Convenience wrapper: returns the negation of cmp_by_gpa().
 *
 * @param  a  Pointer to first Student.
 * @param  b  Pointer to second Student.
 */
int cmp_by_gpa_desc(const void *a, const void *b);

/**
 * @brief  Compare two Student structs by roll_number (ascending).
 * @param  a  Pointer to first Student.
 * @param  b  Pointer to second Student.
 */
int cmp_by_roll(const void *a, const void *b);

/**
 * @brief  Compare two Student structs by student_id (ascending).
 *
 * Used internally by sort_by_id() and as the bsearch() comparator in
 * search_by_id().
 *
 * @param  a  Pointer to first Student.
 * @param  b  Pointer to second Student.
 */
int cmp_by_id(const void *a, const void *b);

/**
 * @brief  Compare two Student structs by department, then by name (ascending).
 *
 * Two-key comparator: primary key is department (case-insensitive strcmp),
 * secondary key is name (case-insensitive strcmp) to break ties.
 *
 * @param  a  Pointer to first Student.
 * @param  b  Pointer to second Student.
 */
int cmp_by_dept_then_name(const void *a, const void *b);

/* =========================================================================
 * Tier 3 — Insertion sort (educational, O(n²))
 * ========================================================================= */

/**
 * @brief  Sort the registry by name using insertion sort (ascending).
 *
 * Insertion sort is included to demonstrate algorithmic awareness.
 * It is efficient for nearly-sorted data (O(n) best case) and is stable.
 * For large datasets, prefer sort_by_name() which uses qsort().
 *
 * Complexity:
 *   - Best case:    O(n)     — already sorted
 *   - Average case: O(n²)
 *   - Worst case:   O(n²)   — reverse sorted
 *   - Space:        O(1)    — in-place
 *
 * @param  reg  Registry to sort in-place. Must not be NULL.
 */
void sort_insertion_by_name(StudentRegistry *reg);

/**
 * @brief  Sort the registry by GPA using insertion sort (descending).
 *
 * Same complexity characteristics as sort_insertion_by_name().
 *
 * @param  reg  Registry to sort in-place. Must not be NULL.
 */
void sort_insertion_by_gpa_desc(StudentRegistry *reg);

/* =========================================================================
 * Interactive sort dispatcher (called from main.c menu)
 * ========================================================================= */

/**
 * @brief  Display a sort sub-menu, collect sort criteria, and sort the
 *         registry accordingly.
 *
 * This is the only function in sort.h that performs I/O. Isolated here so
 * that the other sort functions remain testable without a terminal.
 *
 * @param  reg  Registry to sort in-place. Must not be NULL.
 */
void sort_interactive(StudentRegistry *reg);

#ifdef __cplusplus
}
#endif

#endif /* SORT_H */
