/**
 * @file    student.h
 * @brief   Core data structures and CRUD API for the Student Record System.
 *
 * This header defines the canonical Student record layout, the in-memory
 * StudentRegistry that acts as the single source of truth during a session,
 * and the unified ResultCode enumeration used as the return type across all
 * modules.
 *
 * Design contract:
 *   - All public functions return ResultCode; output is written via pointer
 *     parameters where needed.
 *   - The caller never allocates Student memory; the registry owns it.
 *   - Functions are thread-safe at the documentation level (single-threaded
 *     implementation; the slot is noted for future mutex wrapping).
 *
 * @author  Student Record System
 * @version 1.0.0
 */

#ifndef STUDENT_H
#define STUDENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>   /* time_t */

/* =========================================================================
 * Compile-time configuration constants
 * ========================================================================= */

/** Maximum number of student records held in memory. */
#define MAX_STUDENTS       500

/** Maximum byte lengths (including NUL terminator). */
#define MAX_NAME_LEN        64
#define MAX_DEPT_LEN        32
#define MAX_EMAIL_LEN       64
#define MAX_PHONE_LEN       16
#define MAX_ADDRESS_LEN    128

/** GPA bounds (inclusive). */
#define GPA_MIN            0.0f
#define GPA_MAX           10.0f

/** Academic year bounds (inclusive). */
#define YEAR_MIN            1
#define YEAR_MAX            6

/** Student age bounds (inclusive). */
#define AGE_MIN            16
#define AGE_MAX           100

/* =========================================================================
 * Result / error codes  (propagated across all modules)
 * ========================================================================= */

/**
 * @brief Unified result code returned by every operation in the system.
 *
 * Using a named enum (rather than bare int return values) makes call-site
 * intent explicit and enables exhaustive switch-case coverage with -Wswitch.
 */
typedef enum {
    RESULT_OK              =  0,  /**< Operation completed successfully.         */
    RESULT_ERR_NOT_FOUND   = -1,  /**< Requested record does not exist.          */
    RESULT_ERR_DUPLICATE   = -2,  /**< Duplicate primary key (student_id).       */
    RESULT_ERR_FULL        = -3,  /**< Registry is at MAX_STUDENTS capacity.     */
    RESULT_ERR_INVALID     = -4,  /**< Input failed validation.                  */
    RESULT_ERR_FILE        = -5,  /**< File open / read / write failure.         */
    RESULT_ERR_CORRUPT     = -6,  /**< File header or checksum mismatch.         */
    RESULT_ERR_PERMISSION  = -7,  /**< Insufficient filesystem permissions.      */
    RESULT_ERR_AUTH        = -8,  /**< Authentication failure.                   */
    RESULT_ERR_MEMORY      = -9   /**< Dynamic allocation failure.               */
} ResultCode;

/**
 * @brief  Convert a ResultCode to a human-readable string.
 * @param  code  The result code to translate.
 * @return Pointer to a static, NUL-terminated string. Never NULL.
 */
const char *result_to_string(ResultCode code);

/* =========================================================================
 * Student record
 * ========================================================================= */

/**
 * @brief The canonical student record stored in both memory and on disk.
 *
 * Fields are ordered to minimise struct padding on 32-bit and 64-bit targets.
 * The total size is kept fixed (no pointers, no VLAs) to allow direct binary
 * serialisation via a single fwrite() call per record.
 *
 * Soft-delete pattern:
 *   Records with is_active == 0 are logically deleted but physically retained
 *   until an explicit compaction/purge operation is run. This prevents
 *   accidental data loss and mirrors production database practice.
 */
typedef struct {
    int    student_id;                  /**< Primary key. Unique across all records.    */
    int    roll_number;                 /**< Class/batch roll number.                   */
    int    age;                         /**< Student age  [AGE_MIN, AGE_MAX].           */
    int    year_of_study;               /**< Academic year [YEAR_MIN, YEAR_MAX].        */
    float  gpa;                         /**< Grade Point Average [GPA_MIN, GPA_MAX].    */
    int    is_active;                   /**< Soft-delete flag: 1 = active, 0 = deleted. */
    time_t enrollment_date;             /**< Unix timestamp set on record creation.      */
    char   name[MAX_NAME_LEN];          /**< Full name (first + last).                  */
    char   department[MAX_DEPT_LEN];    /**< Academic department, e.g. "Comp. Science". */
    char   email[MAX_EMAIL_LEN];        /**< Contact email address.                     */
    char   phone[MAX_PHONE_LEN];        /**< Phone number, e.g. "+911234567890".        */
    char   address[MAX_ADDRESS_LEN];    /**< Optional mailing address.                  */
} Student;

/* =========================================================================
 * In-memory student registry
 * ========================================================================= */

/**
 * @brief Single in-memory store for all student records during a session.
 *
 * Ownership model:
 *   - registry_init()  : zeroes the structure and sets sane defaults.
 *   - registry_destroy(): currently a no-op (fixed array), but present for
 *                         forward-compatibility if dynamic allocation is added.
 *   - The module that owns this struct (student.c) exposes access only via
 *     the CRUD API; no external code may access registry internals directly.
 */
typedef struct {
    Student records[MAX_STUDENTS]; /**< Contiguous array — cache-friendly, trivially serialisable. */
    int     count;                 /**< Active records currently in use (is_active == 1).          */
    int     total_ever;            /**< Monotonically increasing counter; drives next_id logic.    */
    int     next_id;               /**< Next available auto-generated student_id.                  */
    int     is_dirty;              /**< 1 = unsaved changes exist; prompt user on exit.            */
} StudentRegistry;

/* =========================================================================
 * Registry lifecycle
 * ========================================================================= */

/**
 * @brief  Initialise a StudentRegistry to a clean, empty state.
 * @param  reg  Pointer to the registry to initialise. Must not be NULL.
 */
void registry_init(StudentRegistry *reg);

/**
 * @brief  Release any resources held by the registry.
 * @param  reg  Pointer to the registry to destroy. Must not be NULL.
 * @note   No-op for the current fixed-array implementation.
 */
void registry_destroy(StudentRegistry *reg);

/* =========================================================================
 * CRUD operations
 * ========================================================================= */

/**
 * @brief  Add a new student record to the registry.
 *
 * Performs duplicate-ID detection and capacity checking before insertion.
 * Marks the registry dirty on success.
 *
 * @param  reg      Target registry. Must not be NULL.
 * @param  student  Populated Student struct to add. ID uniqueness is enforced.
 * @return RESULT_OK           on success.
 * @return RESULT_ERR_DUPLICATE if student_id already exists.
 * @return RESULT_ERR_FULL      if registry is at capacity.
 * @return RESULT_ERR_INVALID   if required fields fail validation.
 */
ResultCode student_add(StudentRegistry *reg, const Student *student);

/**
 * @brief  Retrieve a student record by its primary key (student_id).
 *
 * @param  reg         Source registry. Must not be NULL.
 * @param  student_id  Primary key to look up.
 * @param  out         Output buffer; populated on RESULT_OK. Must not be NULL.
 * @return RESULT_OK           on success.
 * @return RESULT_ERR_NOT_FOUND if no active record matches the ID.
 */
ResultCode student_get_by_id(const StudentRegistry *reg, int student_id, Student *out);

/**
 * @brief  Update an existing student record.
 *
 * Locates the record by student_id from @p updated, validates all changed
 * fields, and replaces the stored record in-place. Marks registry dirty.
 *
 * @param  reg      Target registry. Must not be NULL.
 * @param  updated  Student struct with the new field values. Must not be NULL.
 * @return RESULT_OK           on success.
 * @return RESULT_ERR_NOT_FOUND if no record with that student_id exists.
 * @return RESULT_ERR_INVALID   if any field fails validation.
 */
ResultCode student_update(StudentRegistry *reg, const Student *updated);

/**
 * @brief  Soft-delete a student record by its primary key.
 *
 * Sets is_active = 0 without removing the array slot. Marks registry dirty.
 *
 * @param  reg        Target registry. Must not be NULL.
 * @param  student_id Primary key of the record to delete.
 * @return RESULT_OK           on success.
 * @return RESULT_ERR_NOT_FOUND if no active record matches the ID.
 */
ResultCode student_delete(StudentRegistry *reg, int student_id);

/**
 * @brief  Permanently remove all soft-deleted records and compact the array.
 *
 * After compaction, count reflects only active records and the array has
 * no gaps. This is an irreversible operation.
 *
 * @param  reg  Target registry. Must not be NULL.
 * @return Number of records permanently purged (>= 0).
 */
int student_purge_deleted(StudentRegistry *reg);

/* =========================================================================
 * Display helpers
 * ========================================================================= */

/**
 * @brief  Print a formatted table header for student listings.
 */
void student_print_table_header(void);

/**
 * @brief  Print a single student record as a formatted table row.
 * @param  s      Pointer to the student to display. Must not be NULL.
 * @param  index  1-based display row number printed in the first column.
 */
void student_print_row(const Student *s, int index);

/**
 * @brief  Print a full detailed view of a single student record.
 * @param  s  Pointer to the student to display. Must not be NULL.
 */
void student_print_detail(const Student *s);

/**
 * @brief  Interactively prompt the user to fill in a new Student struct.
 *
 * Uses validation.h functions internally; re-prompts on bad input.
 *
 * @param  out  Output buffer to populate. Must not be NULL.
 * @param  reg  Read-only registry used for duplicate ID checking.
 */
void student_prompt_new(Student *out, const StudentRegistry *reg);

/**
 * @brief  Interactively prompt the user to edit fields of an existing student.
 *
 * Displays current values and allows field-by-field update; empty input
 * retains the existing value.
 *
 * @param  s  Student record to edit in-place. Must not be NULL.
 */
void student_prompt_edit(Student *s);

/**
 * @brief  List all active students with optional pagination.
 *
 * @param  reg           Source registry. Must not be NULL.
 * @param  page_size     Number of records per page (0 = show all).
 */
void student_list_all(const StudentRegistry *reg, int page_size);

/* =========================================================================
 * Statistics helpers (used by reports module in main.c)
 * ========================================================================= */

/**
 * @brief  Return the Student with the highest GPA.
 * @param  reg  Source registry. Must not be NULL.
 * @param  out  Output buffer; populated on success. Must not be NULL.
 * @return RESULT_OK if at least one active record exists, RESULT_ERR_NOT_FOUND otherwise.
 */
ResultCode student_get_top_gpa(const StudentRegistry *reg, Student *out);

/**
 * @brief  Compute the average GPA of all active students.
 * @param  reg  Source registry. Must not be NULL.
 * @param  out  Pointer to float that receives the result.
 * @return RESULT_OK on success, RESULT_ERR_NOT_FOUND if no active records.
 */
ResultCode student_average_gpa(const StudentRegistry *reg, float *out);

/**
 * @brief  Count active students belonging to a specific department.
 * @param  reg   Source registry. Must not be NULL.
 * @param  dept  NUL-terminated department name (case-insensitive). Must not be NULL.
 * @return Number of matching active records (>= 0).
 */
int student_count_by_department(const StudentRegistry *reg, const char *dept);

#ifdef __cplusplus
}
#endif

#endif /* STUDENT_H */
