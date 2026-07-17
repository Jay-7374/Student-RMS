/**
 * @file    validation.c
 * @brief   Input validation and sanitisation implementation.
 *
 * Pure logic — no I/O except val_read_* functions.
 * Compiled into both server and client builds.
 *
 * @author  Student Record System
 * @version 2.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

#include "../common/validation.h"
#include "../common/student.h"

/* =========================================================================
 * Windows password masking
 * ========================================================================= */
#ifdef _WIN32
    #include <conio.h>   /* _getch() */
#else
    #include <termios.h>
    #include <unistd.h>
#endif

/* =========================================================================
 * Generic string predicates
 * ========================================================================= */

int val_is_nonempty(const char *s)
{
    if (!s) return 0;
    while (*s) {
        if (!isspace((unsigned char)*s)) return 1;
        s++;
    }
    return 0;
}

int val_is_within_length(const char *s, size_t maxlen)
{
    if (!s) return 0;
    return strlen(s) <= maxlen;
}

int val_is_printable(const char *s)
{
    if (!s) return 0;
    while (*s) {
        if (!isprint((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* =========================================================================
 * Numeric predicates
 * ========================================================================= */

int val_int_in_range(int value, int min, int max)
{
    return (value >= min && value <= max);
}

int val_float_in_range(float value, float min, float max)
{
    return (value >= min && value <= max);
}

/* =========================================================================
 * Student field predicates
 * ========================================================================= */

int val_is_valid_student_id(int id)
{
    return (id > 0 && id <= 99999999);
}

int val_is_valid_name(const char *name)
{
    if (!name || !val_is_nonempty(name)) return 0;
    if (!val_is_within_length(name, MAX_NAME_LEN - 1)) return 0;

    while (*name) {
        if (!isalpha((unsigned char)*name) &&
            *name != ' ' && *name != '-' && *name != '\'') {
            return 0;
        }
        name++;
    }
    return 1;
}

int val_is_valid_gpa(float gpa)
{
    return val_float_in_range(gpa, GPA_MIN, GPA_MAX);
}

int val_is_valid_email(const char *email)
{
    const char *at;
    const char *dot;

    if (!email || !val_is_nonempty(email)) return 0;
    if (!val_is_within_length(email, MAX_EMAIL_LEN - 1)) return 0;

    /* Exactly one '@' */
    at = strchr(email, '@');
    if (!at || at == email) return 0;
    if (strchr(at + 1, '@') != NULL) return 0;

    /* Domain must have a '.' after '@' */
    dot = strchr(at + 1, '.');
    if (!dot || dot == at + 1 || *(dot + 1) == '\0') return 0;

    /* No whitespace */
    {
        const char *p = email;
        while (*p) {
            if (isspace((unsigned char)*p)) return 0;
            p++;
        }
    }
    return 1;
}

int val_is_valid_phone(const char *phone)
{
    int digits = 0;
    const char *p;

    if (!phone || !val_is_nonempty(phone)) return 0;
    if (!val_is_within_length(phone, MAX_PHONE_LEN - 1)) return 0;

    p = phone;
    if (*p == '+') p++;

    while (*p) {
        if (isdigit((unsigned char)*p)) {
            digits++;
        } else if (*p != ' ' && *p != '-' && *p != '(' && *p != ')') {
            return 0;
        }
        p++;
    }
    return (digits >= 7 && digits <= 15);
}

int val_is_valid_department(const char *dept)
{
    if (!dept || !val_is_nonempty(dept)) return 0;
    return val_is_within_length(dept, MAX_DEPT_LEN - 1);
}

int val_is_valid_age(int age)
{
    return val_int_in_range(age, AGE_MIN, AGE_MAX);
}

int val_is_valid_year(int year)
{
    return val_int_in_range(year, YEAR_MIN, YEAR_MAX);
}

int val_is_valid_roll(int roll)
{
    return (roll > 0);
}

int val_is_valid_password(const char *password)
{
    int has_upper = 0, has_lower = 0, has_digit = 0, has_special = 0;
    const char *specials = "!@#$%^&*";
    size_t len;

    if (!password) return 0;
    len = strlen(password);
    if (len < 8) return 0;

    while (*password) {
        if (isupper((unsigned char)*password)) has_upper = 1;
        else if (islower((unsigned char)*password)) has_lower = 1;
        else if (isdigit((unsigned char)*password)) has_digit = 1;
        else if (strchr(specials, *password)) has_special = 1;
        password++;
    }
    return has_upper && has_lower && has_digit && has_special;
}

/* =========================================================================
 * String sanitisation
 * ========================================================================= */

void val_trim(char *s)
{
    char *start, *end;
    size_t len;

    if (!s || *s == '\0') return;

    /* Trim leading whitespace */
    start = s;
    while (*start && isspace((unsigned char)*start)) start++;

    /* Move trimmed content to start of buffer */
    if (start != s) {
        len = strlen(start);
        memmove(s, start, len + 1);
    }

    /* Trim trailing whitespace */
    if (*s == '\0') return;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
}

void val_to_title_case(char *s)
{
    int new_word = 1;
    if (!s) return;

    while (*s) {
        if (isspace((unsigned char)*s)) {
            new_word = 1;
        } else if (new_word) {
            *s = (char)toupper((unsigned char)*s);
            new_word = 0;
        } else {
            *s = (char)tolower((unsigned char)*s);
        }
        s++;
    }
}

void val_to_lowercase(char *s)
{
    if (!s) return;
    while (*s) {
        *s = (char)tolower((unsigned char)*s);
        s++;
    }
}

/* =========================================================================
 * Safe interactive input helpers
 * ========================================================================= */

int val_read_line(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return 0;

    buf[0] = '\0';
    if (!fgets(buf, (int)buf_size, stdin)) {
        return 0;
    }

    /* Strip trailing newline */
    {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    }

    /* Discard overflow characters */
    if (strlen(buf) == buf_size - 1) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {}
    }
    return 1;
}

void val_read_int(const char *prompt, int min, int max, int *out)
{
    char buf[VAL_LINE_BUFFER];
    char *endptr;
    long val;

    while (1) {
        printf("%s", prompt);
        fflush(stdout);

        if (!val_read_line(buf, sizeof(buf))) continue;

        val = strtol(buf, &endptr, 10);
        if (endptr == buf || *endptr != '\0') {
            printf("  Invalid input. Enter a number between %d and %d.\n",
                   min, max);
            continue;
        }
        if (val < min || val > max) {
            printf("  Out of range. Enter a number between %d and %d.\n",
                   min, max);
            continue;
        }
        *out = (int)val;
        return;
    }
}

void val_read_float(const char *prompt, float min, float max, float *out)
{
    char buf[VAL_LINE_BUFFER];
    char *endptr;
    double val;

    while (1) {
        printf("%s", prompt);
        fflush(stdout);

        if (!val_read_line(buf, sizeof(buf))) continue;

        val = strtod(buf, &endptr);
        if (endptr == buf || *endptr != '\0') {
            printf("  Invalid input. Enter a decimal between %.2f and %.2f.\n",
                   min, max);
            continue;
        }
        if ((float)val < min || (float)val > max) {
            printf("  Out of range. Enter a value between %.2f and %.2f.\n",
                   min, max);
            continue;
        }
        *out = (float)val;
        return;
    }
}

void val_read_password(const char *prompt, char *buf, size_t buf_size)
{
    size_t len = 0;

    printf("%s", prompt);
    fflush(stdout);

#ifdef _WIN32
    /* Windows: use _getch() for character-by-character masked input */
    {
        int c;
        while ((c = _getch()) != '\r' && c != '\n') {
            if (c == '\b') {  /* Backspace */
                if (len > 0) {
                    len--;
                    printf("\b \b");
                    fflush(stdout);
                }
            } else if (c >= 32 && len < buf_size - 1) {
                buf[len++] = (char)c;
                printf("*");
                fflush(stdout);
            }
        }
    }
#else
    /* POSIX: disable terminal echo */
    {
        struct termios old_term, new_term;
        tcgetattr(STDIN_FILENO, &old_term);
        new_term = old_term;
        new_term.c_lflag &= ~(tcflag_t)(ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

        int c;
        while ((c = getchar()) != '\n' && c != EOF && len < buf_size - 1) {
            buf[len++] = (char)c;
            printf("*");
            fflush(stdout);
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    }
#endif

    buf[len] = '\0';
    printf("\n");
}

void val_read_string(const char *prompt, char *buf, size_t buf_size,
                     int allow_empty)
{
    while (1) {
        printf("%s", prompt);
        fflush(stdout);

        if (!val_read_line(buf, buf_size)) {
            if (allow_empty) return;
            continue;
        }

        val_trim(buf);

        if (!allow_empty && !val_is_nonempty(buf)) {
            printf("  This field cannot be empty.\n");
            continue;
        }
        return;
    }
}
