/**
 * @file    file_manager.c
 * @brief   Binary file I/O for students.dat and auth.dat.
 *
 * Atomic write strategy: write to <path>.tmp then rename().
 * On Windows, rename() fails if the destination exists; we use
 * MoveFileExA(MOVEFILE_REPLACE_EXISTING) as a fallback.
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <direct.h>   /* _mkdir */
    #include <io.h>       /* _access */
    #include <windows.h>  /* MoveFileExA */
    #define MKDIR(p) _mkdir(p)
    #define ACCESS(p,m) _access(p, m)
#else
    #include <sys/stat.h>
    #include <unistd.h>
    #define MKDIR(p) mkdir(p, 0755)
    #define ACCESS(p,m) access(p, m)
#endif

#include "../server/file_manager.h"

/* =========================================================================
 * Atomic rename helper
 * ========================================================================= */

static int atomic_rename(const char *tmp_path, const char *dest_path)
{
#ifdef _WIN32
    return MoveFileExA(tmp_path, dest_path,
                       MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
#else
    return rename(tmp_path, dest_path);
#endif
}

/* =========================================================================
 * fm_ensure_data_dir
 * ========================================================================= */

ResultCode fm_ensure_data_dir(void)
{
    if (ACCESS("data", 0) != 0) {
        if (MKDIR("data") != 0) {
            fprintf(stderr, "[FM] Cannot create data/ directory.\n");
            return RESULT_ERR_PERMISSION;
        }
    }
    return RESULT_OK;
}

/* =========================================================================
 * fm_file_exists
 * ========================================================================= */

int fm_file_exists(const char *filepath)
{
    if (!filepath) return 0;
    return (ACCESS(filepath, 0) == 0) ? 1 : 0;
}

/* =========================================================================
 * fm_compute_checksum  — XOR over every byte
 * ========================================================================= */

unsigned int fm_compute_checksum(const void *data, size_t size)
{
    const unsigned char *p = (const unsigned char *)data;
    unsigned int sum = 0xDEADBEEFu;
    size_t i;
    for (i = 0; i < size; i++) {
        sum ^= (unsigned int)p[i];
        /* Rotate left by 1 bit for better avalanche */
        sum = (sum << 1) | (sum >> 31);
    }
    return sum;
}

/* =========================================================================
 * fm_verify_magic
 * ========================================================================= */

int fm_verify_magic(const char *filepath)
{
    FILE *f;
    char  magic[FILE_MAGIC_LEN];

    if (!filepath) return 0;
    f = fopen(filepath, "rb");
    if (!f) return 0;

    if (fread(magic, 1, FILE_MAGIC_LEN, f) != FILE_MAGIC_LEN) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return (memcmp(magic, FILE_MAGIC, FILE_MAGIC_LEN) == 0) ? 1 : 0;
}

/* =========================================================================
 * fm_load_registry
 * ========================================================================= */

ResultCode fm_load_registry(StudentRegistry *reg)
{
    FILE       *f;
    FileHeader  header;
    size_t      read_count;

    if (!reg) return RESULT_ERR_INVALID;

    /* First-run: file does not exist yet */
    if (!fm_file_exists(DATA_FILE_PATH)) {
        fprintf(stdout, "[FM] %s not found. Starting fresh.\n", DATA_FILE_PATH);
        return RESULT_OK;
    }

    f = fopen(DATA_FILE_PATH, "rb");
    if (!f) {
        perror("[FM] fopen(students.dat)");
        return RESULT_ERR_FILE;
    }

    /* Read and validate header */
    if (fread(&header, sizeof(FileHeader), 1, f) != 1) {
        fclose(f);
        return RESULT_ERR_CORRUPT;
    }

    if (memcmp(header.magic, FILE_MAGIC, FILE_MAGIC_LEN) != 0) {
        fprintf(stderr, "[FM] Invalid magic bytes in %s.\n", DATA_FILE_PATH);
        fclose(f);
        return RESULT_ERR_CORRUPT;
    }

    if (header.version != FILE_FORMAT_VERSION) {
        fprintf(stderr, "[FM] File format version mismatch: got %d, expected %d.\n",
                header.version, FILE_FORMAT_VERSION);
        fclose(f);
        return RESULT_ERR_CORRUPT;
    }

    if (header.record_count < 0 || header.record_count > MAX_STUDENTS) {
        fprintf(stderr, "[FM] Implausible record_count=%d in header.\n",
                header.record_count);
        fclose(f);
        return RESULT_ERR_CORRUPT;
    }

    /* Read records */
    read_count = (size_t)header.record_count;
    if (read_count > 0) {
        size_t n = fread(reg->records, sizeof(Student), read_count, f);
        if (n != read_count) {
            fprintf(stderr, "[FM] Truncated file: expected %lu records, got %lu.\n",
                    (unsigned long)read_count, (unsigned long)n);
            fclose(f);
            return RESULT_ERR_CORRUPT;
        }
    }

    fclose(f);

    /* Validate checksum */
    {
        unsigned int computed = fm_compute_checksum(reg->records,
                                                    read_count * sizeof(Student));
        if (read_count > 0 && computed != header.checksum) {
            fprintf(stderr, "[FM] Checksum mismatch. File may be corrupted.\n");
            return RESULT_ERR_CORRUPT;
        }
    }

    /* Recompute active count and next_id from loaded records */
    {
        int i, max_id = 0;
        reg->count = 0;
        for (i = 0; i < (int)read_count; i++) {
            if (reg->records[i].is_active) reg->count++;
            if (reg->records[i].student_id > max_id)
                max_id = reg->records[i].student_id;
        }
        reg->next_id    = max_id + 1;
        reg->total_ever = (int)read_count;
        reg->is_dirty   = 0;
    }

    fprintf(stdout, "[FM] Loaded %d record(s) (%d active).\n",
            header.record_count, reg->count);
    return RESULT_OK;
}

/* =========================================================================
 * fm_save_registry
 * ========================================================================= */

ResultCode fm_save_registry(StudentRegistry *reg)
{
    FILE        *f;
    FileHeader   header;
    char         tmp_path[256];
    int          total_slots;
    size_t       i;

    if (!reg) return RESULT_ERR_INVALID;

    snprintf(tmp_path, sizeof(tmp_path), "%s%s", DATA_FILE_PATH, TEMP_FILE_SUFFIX);

    f = fopen(tmp_path, "wb");
    if (!f) {
        perror("[FM] fopen(students.dat.tmp)");
        return RESULT_ERR_FILE;
    }

    /* Count total used slots (active + soft-deleted) */
    total_slots = 0;
    for (i = 0; i < MAX_STUDENTS; i++) {
        if (reg->records[i].student_id != 0) total_slots++;
    }

    /* Build header */
    memset(&header, 0, sizeof(FileHeader));
    memcpy(header.magic,    FILE_MAGIC,  FILE_MAGIC_LEN);
    header.version        = FILE_FORMAT_VERSION;
    header.record_count   = total_slots;
    header.active_count   = reg->count;
    header.next_id        = reg->next_id;
    header.last_modified  = time(NULL);
    header.checksum       = fm_compute_checksum(reg->records,
                                                (size_t)total_slots * sizeof(Student));

    if (fwrite(&header, sizeof(FileHeader), 1, f) != 1) {
        fclose(f);
        remove(tmp_path);
        return RESULT_ERR_FILE;
    }

    if (total_slots > 0) {
        if (fwrite(reg->records, sizeof(Student), (size_t)total_slots, f)
                != (size_t)total_slots) {
            fclose(f);
            remove(tmp_path);
            return RESULT_ERR_FILE;
        }
    }

    fclose(f);

    /* Atomic rename: tmp → final */
    if (atomic_rename(tmp_path, DATA_FILE_PATH) != 0) {
        perror("[FM] rename(students.dat.tmp -> students.dat)");
        remove(tmp_path);
        return RESULT_ERR_FILE;
    }

    reg->is_dirty = 0;
    fprintf(stdout, "[FM] Saved %d record(s) to %s.\n",
            total_slots, DATA_FILE_PATH);
    return RESULT_OK;
}

/* =========================================================================
 * Password hashing (djb2 salted)
 * ========================================================================= */

void fm_generate_salt(char *out, int length)
{
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#";
    int i;

    srand((unsigned int)time(NULL) ^ (unsigned int)(size_t)out);
    for (i = 0; i < length; i++) {
        out[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    out[length] = '\0';
}

void fm_hash_password(const char *password, const char *salt, char *out_hash)
{
    unsigned long hash = 5381UL;
    const char   *p;
    int           i;

    /* Hash salt first */
    for (p = salt; *p; p++) {
        hash = ((hash << 5) + hash) ^ (unsigned long)(unsigned char)*p;
    }
    /* Hash password */
    for (p = password; *p; p++) {
        hash = ((hash << 5) + hash) ^ (unsigned long)(unsigned char)*p;
    }

    /* Write as 16-character hex string (lower 64 bits) */
    for (i = 0; i < 16; i++) {
        unsigned char nibble = (unsigned char)((hash >> ((15 - i) * 4)) & 0x0F);
        out_hash[i] = "0123456789abcdef"[nibble];
    }
    /* Pad to 64 chars for the field size */
    for (i = 16; i < 64; i++) out_hash[i] = '0';
    out_hash[64] = '\0';
}

int fm_verify_password(const char *password, const AdminCredentials *creds)
{
    char computed[65];
    if (!password || !creds) return 0;
    fm_hash_password(password, creds->salt, computed);
    return (strcmp(computed, creds->password_hash) == 0) ? 1 : 0;
}

/* =========================================================================
 * Credential persistence
 * ========================================================================= */

ResultCode fm_load_credentials(AdminCredentials *creds)
{
    FILE *f;

    if (!creds) return RESULT_ERR_INVALID;

    if (!fm_file_exists(AUTH_FILE_PATH)) {
        /* First run — create default credentials */
        memset(creds, 0, sizeof(AdminCredentials));
        strncpy(creds->username, AUTH_DEFAULT_USER, sizeof(creds->username) - 1);
        fm_generate_salt(creds->salt, AUTH_SALT_LEN);
        fm_hash_password(AUTH_DEFAULT_PASS, creds->salt, creds->password_hash);
        creds->is_first_run    = 1;
        creds->max_attempts    = AUTH_MAX_ATTEMPTS;
        creds->failed_attempts = 0;
        creds->last_login      = 0;
        return fm_save_credentials(creds);
    }

    f = fopen(AUTH_FILE_PATH, "rb");
    if (!f) return RESULT_ERR_FILE;

    if (fread(creds, sizeof(AdminCredentials), 1, f) != 1) {
        fclose(f);
        return RESULT_ERR_CORRUPT;
    }
    fclose(f);
    return RESULT_OK;
}

ResultCode fm_save_credentials(const AdminCredentials *creds)
{
    FILE *f;
    char  tmp[256];

    if (!creds) return RESULT_ERR_INVALID;

    snprintf(tmp, sizeof(tmp), "%s%s", AUTH_FILE_PATH, TEMP_FILE_SUFFIX);

    f = fopen(tmp, "wb");
    if (!f) return RESULT_ERR_FILE;

    if (fwrite(creds, sizeof(AdminCredentials), 1, f) != 1) {
        fclose(f);
        remove(tmp);
        return RESULT_ERR_FILE;
    }
    fclose(f);

    if (atomic_rename(tmp, AUTH_FILE_PATH) != 0) {
        remove(tmp);
        return RESULT_ERR_FILE;
    }
    return RESULT_OK;
}
