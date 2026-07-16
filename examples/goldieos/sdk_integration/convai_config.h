/**
 * @file convai_config.h
 * @brief Configuration file reader for ConvAI bridge.
 *
 * Reads key=value pairs from a config file located alongside the executable.
 * No external JSON dependency — pure C string parsing.
 *
 * Config file format (space or newline separated):
 *   auth_type=0
 *   product_id=941dd1a1-bd94-4655-8b16-a46645e1d9b6
 *   product_key=AbCdEfGhIjKlMnOpQr09
 *   product_secret=AbCdEfGhIjKlMnOpQrStUvWxYz1234567890ABCD
 *   device_name=device1
 *   agent_id=00e4241c-e87c-49a4-a91c-1c58ba208ba9
 */

#ifndef CONVAI_CONFIG_H
#define CONVAI_CONFIG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- config value getters ---- */

/** Get auth_type string, or NULL if not set. */
const char *convai_config_get_auth_type(void);

/** Get product_id string, or NULL if not set. */
const char *convai_config_get_product_id(void);

/** Get product_key string, or NULL if not set. */
const char *convai_config_get_product_key(void);

/** Get product_secret string, or NULL if not set. */
const char *convai_config_get_product_secret(void);

/** Get device_name string, or NULL if not set. */
const char *convai_config_get_device_name(void);

/** Get agent_id string, or NULL if not set. */
const char *convai_config_get_agent_id(void);

/**
 * Get a raw config value by key, or NULL if not found.
 * Used for custom / future keys not covered by the typed getters above.
 */
const char *convai_config_get(const char *key);

/* ---- lifecycle ---- */

/**
 * Initialise the config module.
 *
 * By default, reads "convai.cfg" from the same directory as the
 * current executable.  On embedded platforms without an executable
 * path, call convai_config_init_path() instead.
 *
 * @return 0 on success, -1 if the file could not be opened.
 */
int convai_config_init(void);

/**
 * Initialise the config module from an explicit file path.
 *
 * @param path  Absolute or relative path to the config file.
 * @return 0 on success, -1 if the file could not be opened.
 */
int convai_config_init_path(const char *path);

/**
 * Release all resources held by the config module.
 * Safe to call even if never initialised.
 */
void convai_config_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* CONVAI_CONFIG_H */
