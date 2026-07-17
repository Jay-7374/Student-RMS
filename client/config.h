/**
 * @file    config.h
 * @brief   Client configuration loader — reads config.ini at startup.
 *
 * This module is a singleton. Call cfg_load() once at program start,
 * then use cfg_get_host() / cfg_get_port() throughout the application.
 *
 * config.ini format:
 * ─────────────────
 *   # Lines beginning with '#' are comments and are ignored.
 *   # Blank lines are ignored.
 *   # Keys and values are trimmed of leading/trailing whitespace.
 *
 *   SERVER_HOST=studentdb.duckdns.org
 *   SERVER_PORT=8080
 *
 * Behaviour when config.ini is missing:
 *   cfg_load() returns -1 and prints a warning. The module falls back to
 *   compile-time defaults (DEFAULT_HOST / DEFAULT_PORT from protocol.h)
 *   so the client can still function on a local machine.
 *
 * Future extension:
 *   Add keys here and a matching getter — no other files need to change.
 *   Example future keys:
 *       LOG_LEVEL=debug
 *       TLS_CERT=./certs/server.crt
 *       RECONNECT_ATTEMPTS=3
 *
 * @author  Student Record System
 * @version 2.1.0
 */

#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Constants
 * ========================================================================= */

/** Maximum length of a hostname (RFC 1035 limit is 253). */
#define CFG_MAX_HOST_LEN    256

/** Maximum length of a single config.ini line (key + '=' + value). */
#define CFG_MAX_LINE_LEN    512

/** Default config file name searched in the current working directory. */
#define CFG_DEFAULT_FILE    "config.ini"

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

/**
 * @brief  Load configuration from a file.
 *
 * Parses the file at @p filepath for recognised key=value pairs.
 * Unknown keys are silently ignored (forward-compatible). Inline comments
 * (# after a value) are also stripped.
 *
 * If the file does not exist or cannot be opened, built-in defaults are
 * kept active and -1 is returned. The client may choose to treat this as
 * a non-fatal warning.
 *
 * @param  filepath  Path to the config file. Pass NULL to use the default
 *                   filename "config.ini" in the current directory.
 * @return  0  on success (file parsed, at least one key loaded).
 * @return -1  if the file was not found or could not be opened.
 * @return -2  if the file exists but contains no recognised keys.
 */
int cfg_load(const char *filepath);

/* =========================================================================
 * Getters
 * ========================================================================= */

/**
 * @brief  Return the configured server hostname or IP string.
 *
 * Suitable for passing directly to getaddrinfo().
 *
 * @return  NUL-terminated hostname. Never NULL. Falls back to DEFAULT_HOST
 *          if cfg_load() has not been called or failed.
 */
const char *cfg_get_host(void);

/**
 * @brief  Return the configured server TCP port.
 *
 * @return  Port number in range [1, 65535]. Falls back to DEFAULT_PORT
 *          if cfg_load() has not been called or failed.
 */
int cfg_get_port(void);

/* =========================================================================
 * Diagnostics
 * ========================================================================= */

/**
 * @brief  Print the currently active configuration to stdout.
 *
 * Useful for showing the user what settings are active at startup.
 * Example output:
 *   [Config] HOST : studentdb.duckdns.org
 *   [Config] PORT : 8080
 *   [Config] SRC  : config.ini
 */
void cfg_print(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
