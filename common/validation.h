/**
 * @file    validation.h
 * @brief   Input validation and sanitisation API for the Student Record System.
 *
 * Every function in this module is a pure predicate or transformer:
 *   - Predicates return 1 (valid) or 0 (invalid).
 *   - Transformers write sanitised output into a caller-provided buffer
 *     and return 1 on success or 0 on failure.
 *
 * No I/O is performed here. Validation is kept strictly separated from
 * user interaction so that unit tests can call these functions directly
 * without requiring a terminal.
 *
 * Naming convention:
 *   - val_is_*   : Returns 1 if input satisfies the constraint, 0 otherwise.
 *   - val_sanitise_* : Trims/normalises input into out buffer; returns 1/0.
 *   - val_read_* : Reads a line from stdin, validates, re-prompts on failure.
 *
 * @author  Student Record System
 * @version 1.0.0
 */

#ifndef VALIDATION_H
#define VALIDATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>    /* size_t */

/* =========================================================================
 * Buffer size for safe line reading
 * ========================================================================= */

/** Maximum bytes read in a single val_read_line() call. */
#define VAL_LINE_BUFFER   256

/* =========================================================================
 * Generic string predicates
 * ========================================================================= */

/**
 * @brief  Check that a string is non-empty after trimming whitespace.
 * @param  s  NUL-terminated string to test. Must not be NULL.
 * @return 1 if the trimmed string has at least one character, 0 otherwise.
 */
int val_is_nonempty(const char *s);

/**
 * @brief  Check that a string does not exceed a given length.
 * @param  s       NUL-terminated string. Must not be NULL.
 * @param  maxlen  Maximum allowed length (not including NUL terminator).
 * @return 1 if strlen(s) <= maxlen, 0 otherwise.
 */
int val_is_within_length(const char *s, size_t maxlen);

/**
 * @brief  Check that a string contains only printable characters.
 * @param  s  NUL-terminated string. Must not be NULL.
 * @return 1 if all characters are printable (isprint), 0 otherwise.
 */
int val_is_printable(const char *s);

/* =========================================================================
 * Numeric predicates
 * ========================================================================= */

/**
 * @brief  Check that an integer falls within [min, max] (inclusive).
 * @param  value  Integer value to test.
 * @param  min    Lower bound (inclusive).
 * @param  max    Upper bound (inclusive).
 * @return 1 if min <= value <= max, 0 otherwise.
 */
int val_int_in_range(int value, int min, int max);

/**
 * @brief  Check that a float falls within [min, max] (inclusive).
 * @param  value  Float value to test.
 * @param  min    Lower bound (inclusive).
 * @param  max    Upper bound (inclusive).
 * @return 1 if min <= value <= max, 0 otherwise.
 */
int val_float_in_range(float value, float min, float max);

/* =========================================================================
 * Student field predicates
 * ========================================================================= */

/**
 * @brief  Validate a student ID.
 *
 * A valid student ID is a positive integer with at most 8 digits.
 *
 * @param  id  Student ID to validate.
 * @return 1 if valid, 0 otherwise.
 */
int val_is_valid_student_id(int id);

/**
 * @brief  Validate a student name.
 *
 * Rules:
 *   - Non-empty after trimming.
 *   - Contains only letters, spaces, hyphens, and apostrophes.
 *   - Does not exceed MAX_NAME_LEN - 1 characters.
 *
 * @param  name  NUL-terminated name string. Must not be NULL.
 * @return 1 if valid, 0 otherwise.
 */
int val_is_valid_name(const char *name);

/**
 * @brief  Validate a GPA value.
 * @param  gpa  GPA to validate.
 * @return 1 if GPA_MIN <= gpa <= GPA_MAX, 0 otherwise.
 */
int val_is_valid_gpa(float gpa);

/**
 * @brief  Validate an email address (basic RFC 5321 surface check).
 *
 * Rules:
 *   - Contains exactly one '@'.
 *   - Local part (before '@') is non-empty.
 *   - Domain (after '@') contains at least one '.'.
 *   - No whitespace.
 *   - Total length <= MAX_EMAIL_LEN - 1.
 *
 * @param  email  NUL-terminated email string. Must not be NULL.
 * @return 1 if valid, 0 otherwise.
 */
int val_is_valid_email(const char *email);

/**
 * @brief  Validate a phone number.
 *
 * Rules:
 *   - Optional leading '+'.
 *   - Remaining characters are digits, spaces, hyphens, or parentheses.
 *   - Total digit count is between 7 and 15 (ITU-T E.164).
 *   - Total length <= MAX_PHONE_LEN - 1.
 *
 * @param  phone  NUL-terminated phone string. Must not be NULL.
 * @return 1 if valid, 0 otherwise.
 */
int val_is_valid_phone(const char *phone);

/**
 * @brief  Validate a department name.
 *
 * Rules:
 *   - Non-empty after trimming.
 *   - Does not exceed MAX_DEPT_LEN - 1 characters.
 *
 * @param  dept  NUL-terminated department string. Must not be NULL.
 * @return 1 if valid, 0 otherwise.
 */
int val_is_valid_department(const char *dept);

/**
 * @brief  Validate a student age.
 * @param  age  Age value to validate.
 * @return 1 if AGE_MIN <= age <= AGE_MAX, 0 otherwise.
 */
int val_is_valid_age(int age);

/**
 * @brief  Validate a year of study.
 * @param  year  Year value to validate.
 * @return 1 if YEAR_MIN <= year <= YEAR_MAX, 0 otherwise.
 */
int val_is_valid_year(int year);

/**
 * @brief  Validate a roll number.
 * @param  roll  Roll number to validate.
 * @return 1 if roll > 0, 0 otherwise.
 */
int val_is_valid_roll(int roll);

/**
 * @brief  Validate an admin password.
 *
 * Rules (minimum strength requirements):
 *   - At least 8 characters.
 *   - At least one uppercase letter.
 *   - At least one lowercase letter.
 *   - At least one digit.
 *   - At least one special character (!@#$%^&*).
 *
 * @param  password  NUL-terminated password. Must not be NULL.
 * @return 1 if all strength rules are met, 0 otherwise.
 */
int val_is_valid_password(const char *password);

/* =========================================================================
 * String sanitisation
 * ========================================================================= */

/**
 * @brief  Trim leading and trailing whitespace from a string in-place.
 *
 * Modifies the string directly; moves content, does not reallocate.
 *
 * @param  s  NUL-terminated string to trim. Must not be NULL.
 */
void val_trim(char *s);

/**
 * @brief  Convert a string to title case in-place (first letter of each word).
 * @param  s  NUL-terminated string. Must not be NULL.
 */
void val_to_title_case(char *s);

/**
 * @brief  Convert a string to lowercase in-place.
 * @param  s  NUL-terminated string. Must not be NULL.
 */
void val_to_lowercase(char *s);

/* =========================================================================
 * Safe interactive input helpers
 * ========================================================================= */

/**
 * @brief  Read a line from stdin safely into a caller-provided buffer.
 *
 * Discards any characters beyond (buf_size - 1) to prevent buffer overflows.
 * Strips the trailing newline. On EOF or error, writes an empty string and
 * returns 0.
 *
 * @param  buf       Output buffer. Must not be NULL.
 * @param  buf_size  Capacity of buf in bytes (including NUL terminator).
 * @return 1 on success, 0 on EOF or I/O error.
 */
int val_read_line(char *buf, size_t buf_size);

/**
 * @brief  Read and validate an integer from stdin, re-prompting on error.
 *
 * @param  prompt    Text displayed before reading. Must not be NULL.
 * @param  min       Minimum acceptable value (inclusive).
 * @param  max       Maximum acceptable value (inclusive).
 * @param  out       Pointer that receives the validated integer. Must not be NULL.
 */
void val_read_int(const char *prompt, int min, int max, int *out);

/**
 * @brief  Read and validate a float from stdin, re-prompting on error.
 *
 * @param  prompt    Text displayed before reading. Must not be NULL.
 * @param  min       Minimum acceptable value (inclusive).
 * @param  max       Maximum acceptable value (inclusive).
 * @param  out       Pointer that receives the validated float. Must not be NULL.
 */
void val_read_float(const char *prompt, float min, float max, float *out);

/**
 * @brief  Read a masked password from stdin (characters echoed as '*').
 *
 * Uses platform-appropriate terminal control:
 *   - POSIX: tcgetattr / tcsetattr to disable ECHO.
 *   - Windows: _getch() from <conio.h>.
 * Falls back to plain fgets() if terminal control is unavailable.
 *
 * @param  prompt   Text displayed before reading. Must not be NULL.
 * @param  buf      Output buffer. Must not be NULL.
 * @param  buf_size Capacity of buf in bytes (including NUL terminator).
 */
void val_read_password(const char *prompt, char *buf, size_t buf_size);

/**
 * @brief  Read a string with a label, re-prompting if empty is not allowed.
 *
 * @param  prompt        Display prompt. Must not be NULL.
 * @param  buf           Output buffer. Must not be NULL.
 * @param  buf_size      Capacity of buf.
 * @param  allow_empty   If 1, an empty entry is accepted; if 0, re-prompts.
 */
void val_read_string(const char *prompt, char *buf, size_t buf_size, int allow_empty);

#ifdef __cplusplus
}
#endif

#endif /* VALIDATION_H */
