/**
 * @file    file_manager.h
 * @brief   Binary file I/O, admin authentication persistence, and data
 *          integrity checking for the Student Record System.
 *
 * Responsibilities:
 *   - Define the on-disk binary format (FileHeader + Student[]).
 *   - Provide atomic load/save operations for the StudentRegistry.
 *   - Manage AdminCredentials persistence (salted hash, lockout state).
 *   - Validate file integrity via magic bytes and XOR checksum.
 *   - Handle missing, corrupted, and permission-denied file scenarios
 *     with meaningful ResultCode values.
 *
 * Atomic write strategy:
 *   All save operations write to a "<filename>.tmp" file first, then use
 *   rename() to replace the target atomically. This prevents half-written
 *   files on power loss or program crash.
 *
 * @author  Student Record System
 * @version 1.0.0
 */

#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>           /* time_t   */
#include "../common/student.h"        /* Student, StudentRegistry, ResultCode */

/* =========================================================================
 * File path configuration
 * ========================================================================= */

/** Path to the main student data binary file. */
#define DATA_FILE_PATH      "data/students.dat"

/** Path to the admin credentials binary file. */
#define AUTH_FILE_PATH      "data/auth.dat"

/** Temporary file suffix for atomic writes. */
#define TEMP_FILE_SUFFIX    ".tmp"

/** File format version — increment when the on-disk layout changes. */
#define FILE_FORMAT_VERSION  1

/** Magic bytes written at the start of every data file. */
#define FILE_MAGIC          "SRMS"
#define FILE_MAGIC_LEN       4

/* =========================================================================
 * On-disk structures
 * ========================================================================= */

/**
 * @brief  Binary file header written at byte offset 0 of students.dat.
 *
 * On load, the system validates:
 *   1. magic[]     == FILE_MAGIC        (reject foreign files)
 *   2. version     == FILE_FORMAT_VERSION (reject old/future formats)
 *   3. checksum    matches computed value (reject corrupted data)
 *   4. record_count <= MAX_STUDENTS     (reject overflow attacks)
 */
typedef struct {
    char         magic[FILE_MAGIC_LEN]; /**< Magic identifier bytes: "SRMS".            */
    int          version;               /**< On-disk format version.                     */
    int          record_count;          /**< Total records written (active + deleted).   */
    int          active_count;          /**< Active records at time of save.             */
    int          next_id;               /**< Next auto-generated student ID.             */
    time_t       last_modified;         /**< Unix timestamp of last successful save.     */
    unsigned int checksum;              /**< XOR checksum over the record data block.    */
    char         _padding[8];           /**< Reserved for future fields; zero-filled.   */
} FileHeader;

/* =========================================================================
 * Admin credentials
 * ========================================================================= */

/** Length of the random salt used in password hashing. */
#define AUTH_SALT_LEN       16

/** Maximum allowed consecutive failed login attempts before lockout. */
#define AUTH_MAX_ATTEMPTS    3

/** Default admin username (overridden on first-run setup). */
#define AUTH_DEFAULT_USER   "admin"

/** Default admin password (user is forced to change on first run). */
#define AUTH_DEFAULT_PASS   "Admin@1234"

/**
 * @brief  Admin authentication record stored in auth.dat.
 *
 * The password is never stored in plaintext. On write, the cleartext
 * password is salted with a random AUTH_SALT_LEN byte string, then
 * hashed via djb2_salted_hash() before being stored as a hex string.
 *
 * NOTE: djb2 is used here for portability (no OpenSSL dependency) and is
 * documented as a demo-grade implementation. The struct is designed so that
 * password_hash[] can be replaced with a 64-char SHA-256 hex output with
 * zero changes to the layout.
 */
typedef struct {
    char         username[32];          /**< Admin login username.                       */
    char         password_hash[65];     /**< Hex-encoded salted hash (64 hex + NUL).    */
    char         salt[AUTH_SALT_LEN + 1]; /**< Random salt appended before hashing.     */
    int          is_first_run;          /**< 1 = default credentials; prompt to change. */
    int          failed_attempts;       /**< Consecutive failed login counter.           */
    int          max_attempts;          /**< Lock-out threshold (default: AUTH_MAX_ATTEMPTS). */
    time_t       last_login;            /**< Unix timestamp of last successful login.    */
} AdminCredentials;

/* =========================================================================
 * Registry persistence
 * ========================================================================= */

/**
 * @brief  Load the StudentRegistry from the binary data file.
 *
 * Operation sequence:
 *   1. Open DATA_FILE_PATH for reading.
 *   2. Read and validate FileHeader (magic, version, checksum).
 *   3. Read record_count Student records into reg->records[].
 *   4. Recompute active count and next_id from loaded data.
 *
 * If the file does not exist, the registry is initialised to empty and
 * RESULT_OK is returned (first-run case).
 *
 * @param  reg  Output registry to populate. Must not be NULL.
 * @return RESULT_OK           on success or first-run (empty file).
 * @return RESULT_ERR_CORRUPT   if magic, version, or checksum fails.
 * @return RESULT_ERR_FILE      if the file exists but cannot be read.
 * @return RESULT_ERR_PERMISSION if the process lacks read access.
 */
ResultCode fm_load_registry(StudentRegistry *reg);

/**
 * @brief  Atomically save the StudentRegistry to the binary data file.
 *
 * Operation sequence:
 *   1. Write FileHeader + all records to DATA_FILE_PATH TEMP_FILE_SUFFIX.
 *   2. Compute and embed checksum in the header.
 *   3. rename() temp file over DATA_FILE_PATH (atomic on POSIX; best-effort
 *      on Windows via MoveFileExA with MOVEFILE_REPLACE_EXISTING).
 *   4. Clear reg->is_dirty on success.
 *
 * @param  reg  Source registry to persist. Must not be NULL.
 * @return RESULT_OK            on success.
 * @return RESULT_ERR_FILE       if the temp file cannot be created/written.
 * @return RESULT_ERR_PERMISSION if the directory is not writable.
 */
ResultCode fm_save_registry(StudentRegistry *reg);

/* =========================================================================
 * Admin credentials persistence
 * ========================================================================= */

/**
 * @brief  Load AdminCredentials from auth.dat.
 *
 * If auth.dat does not exist, writes a default credential set and returns
 * RESULT_OK with creds->is_first_run == 1 to prompt the caller to force a
 * password change.
 *
 * @param  creds  Output credentials struct. Must not be NULL.
 * @return RESULT_OK            on success.
 * @return RESULT_ERR_FILE       if the file exists but cannot be read.
 * @return RESULT_ERR_CORRUPT    if the file is malformed.
 */
ResultCode fm_load_credentials(AdminCredentials *creds);

/**
 * @brief  Save AdminCredentials to auth.dat (atomic write).
 * @param  creds  Credentials to persist. Must not be NULL.
 * @return RESULT_OK            on success.
 * @return RESULT_ERR_FILE       on write failure.
 */
ResultCode fm_save_credentials(const AdminCredentials *creds);

/* =========================================================================
 * File integrity utilities
 * ========================================================================= */

/**
 * @brief  Compute an XOR checksum over a block of bytes.
 *
 * Simple but fast integrity check sufficient for detecting file truncation,
 * partial writes, and naive tampering.
 *
 * @param  data   Pointer to the data block. Must not be NULL.
 * @param  size   Number of bytes to checksum.
 * @return Unsigned 32-bit XOR checksum.
 */
unsigned int fm_compute_checksum(const void *data, size_t size);

/**
 * @brief  Verify that a file begins with the expected magic bytes.
 * @param  filepath  Path to the file to inspect. Must not be NULL.
 * @return 1 if magic matches, 0 otherwise.
 */
int fm_verify_magic(const char *filepath);

/**
 * @brief  Check whether a file exists and is readable.
 * @param  filepath  Path to test. Must not be NULL.
 * @return 1 if the file exists and is readable, 0 otherwise.
 */
int fm_file_exists(const char *filepath);

/**
 * @brief  Ensure the data/ directory exists; create it if absent.
 * @return RESULT_OK on success, RESULT_ERR_PERMISSION on creation failure.
 */
ResultCode fm_ensure_data_dir(void);

/* =========================================================================
 * Password hashing
 * ========================================================================= */

/**
 * @brief  Produce a salted hash of a plaintext password using djb2.
 *
 * The salt is prepended to the password before hashing, and the result is
 * written as a lowercase hex string into @p out_hash.
 *
 * @param  password   NUL-terminated plaintext password. Must not be NULL.
 * @param  salt       NUL-terminated salt string. Must not be NULL.
 * @param  out_hash   Output buffer of at least 65 bytes. Must not be NULL.
 */
void fm_hash_password(const char *password, const char *salt, char *out_hash);

/**
 * @brief  Generate a random printable salt string.
 * @param  out      Output buffer of at least AUTH_SALT_LEN + 1 bytes.
 * @param  length   Number of salt characters to generate.
 */
void fm_generate_salt(char *out, int length);

/**
 * @brief  Verify a plaintext password against stored credentials.
 * @param  password   Cleartext password to test. Must not be NULL.
 * @param  creds      Stored credentials to check against. Must not be NULL.
 * @return 1 if the password is correct, 0 otherwise.
 */
int fm_verify_password(const char *password, const AdminCredentials *creds);

#ifdef __cplusplus
}
#endif

#endif /* FILE_MANAGER_H */
