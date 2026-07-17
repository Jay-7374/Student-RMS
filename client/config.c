/**
 * @file    config.c
 * @brief   Client configuration loader implementation.
 *
 * Parsing rules:
 *   1. Lines beginning with '#' (after trimming) are comments — skipped.
 *   2. Blank lines are skipped.
 *   3. Each recognised line must have the form:  KEY=VALUE
 *      Whitespace around the '=' and at line ends is stripped.
 *   4. Inline comments after the value (# ...) are stripped.
 *   5. Unrecognised keys are silently ignored.
 *   6. The last occurrence of a key wins (allows overrides at end of file).
 *
 * Thread safety:
 *   cfg_load() is intended to be called once from main() before any
 *   threads are started. The getters are read-only after that point
 *   and require no locking.
 *
 * @author  Student Record System
 * @version 2.1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "../common/protocol.h"   /* DEFAULT_HOST, DEFAULT_PORT */

/* =========================================================================
 * Module-private state  (singleton)
 * ========================================================================= */

/** Active server hostname — initialised to compile-time default. */
static char g_host[CFG_MAX_HOST_LEN] = {0};

/** Active server port — initialised to compile-time default. */
static int  g_port = 0;

/** Source description used in cfg_print() output. */
static char g_source[CFG_MAX_HOST_LEN] = "built-in defaults";

/** 1 after a successful cfg_load(); guards re-initialisation. */
static int  g_loaded = 0;

/* =========================================================================
 * Internal string helpers
 * ========================================================================= */

/** Trim leading whitespace in-place; returns pointer to first non-space. */
static char *ltrim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/** Trim trailing whitespace and optional inline # comment in-place. */
static void rtrim_and_strip_comment(char *s)
{
    /* Strip inline comment first */
    char *comment = strchr(s, '#');
    if (comment) *comment = '\0';

    /* Strip trailing whitespace */
    if (*s == '\0') return;
    char *end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
}

/** Trim both ends. Returns pointer into the original buffer. */
static char *trim(char *s)
{
    s = ltrim(s);
    rtrim_and_strip_comment(s);
    return s;
}

/* =========================================================================
 * Initialise defaults
 * ========================================================================= */

static void init_defaults(void)
{
    strncpy(g_host, DEFAULT_HOST, CFG_MAX_HOST_LEN - 1);
    g_host[CFG_MAX_HOST_LEN - 1] = '\0';
    g_port = DEFAULT_PORT;
}

/* =========================================================================
 * cfg_load()
 * ========================================================================= */

int cfg_load(const char *filepath)
{
    FILE *f;
    char  line[CFG_MAX_LINE_LEN];
    char  key[CFG_MAX_LINE_LEN];
    char  value[CFG_MAX_LINE_LEN];
    int   keys_loaded = 0;
    int   lineno      = 0;
    const char *path;

    /* Always seed defaults before parsing so partial files still work */
    init_defaults();

    path = (filepath && *filepath) ? filepath : CFG_DEFAULT_FILE;

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr,
                "[Config] WARNING: '%s' not found. "
                "Using built-in defaults (%s:%d).\n",
                path, g_host, g_port);
        strncpy(g_source, "built-in defaults", sizeof(g_source) - 1);
        return -1;
    }

    while (fgets(line, sizeof(line), f)) {
        char *p;
        char *eq;

        lineno++;

        /* Work on a trimmed copy */
        p = trim(line);

        /* Skip blank lines and comments */
        if (*p == '\0' || *p == '#') continue;

        /* Split on first '=' */
        eq = strchr(p, '=');
        if (!eq) {
            /* No '=' — malformed line, skip silently */
            continue;
        }

        /* Extract key */
        size_t key_len = (size_t)(eq - p);
        if (key_len == 0 || key_len >= sizeof(key)) continue;
        memcpy(key, p, key_len);
        key[key_len] = '\0';

        /* Trim key */
        char *ktrimmed = trim(key);

        /* Extract value (everything after '=') */
        strncpy(value, eq + 1, sizeof(value) - 1);
        value[sizeof(value) - 1] = '\0';
        char *vtrimmed = trim(value);

        /* ── Dispatch recognised keys ───────────────────────────── */
        if (strcmp(ktrimmed, "SERVER_HOST") == 0) {
            if (*vtrimmed) {
                strncpy(g_host, vtrimmed, CFG_MAX_HOST_LEN - 1);
                g_host[CFG_MAX_HOST_LEN - 1] = '\0';
                keys_loaded++;
            }
        }
        else if (strcmp(ktrimmed, "SERVER_PORT") == 0) {
            char  *endptr;
            long   port_val = strtol(vtrimmed, &endptr, 10);
            if (*vtrimmed && *endptr == '\0' &&
                port_val > 0 && port_val <= 65535) {
                g_port = (int)port_val;
                keys_loaded++;
            } else {
                fprintf(stderr,
                        "[Config] Line %d: invalid SERVER_PORT '%s' — "
                        "using default %d.\n",
                        lineno, vtrimmed, DEFAULT_PORT);
            }
        }
        /* Future keys go here — no other files need modification */
    }

    fclose(f);

    strncpy(g_source, path, sizeof(g_source) - 1);
    g_source[sizeof(g_source) - 1] = '\0';
    g_loaded = 1;

    if (keys_loaded == 0) {
        fprintf(stderr,
                "[Config] '%s' contained no recognised keys. "
                "Using built-in defaults.\n", path);
        return -2;
    }

    return 0;
}

/* =========================================================================
 * Getters
 * ========================================================================= */

const char *cfg_get_host(void)
{
    if (!g_loaded && g_host[0] == '\0') {
        init_defaults();
    }
    return g_host;
}

int cfg_get_port(void)
{
    if (!g_loaded && g_port == 0) {
        init_defaults();
    }
    return g_port;
}

/* =========================================================================
 * Diagnostics
 * ========================================================================= */

void cfg_print(void)
{
    if (!g_loaded && g_host[0] == '\0') {
        init_defaults();
    }
    fprintf(stdout,
            "[Config] HOST : %s\n"
            "[Config] PORT : %d\n"
            "[Config] SRC  : %s\n",
            g_host, g_port, g_source);
}
