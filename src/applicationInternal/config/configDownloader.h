#pragma once

#include <string>

namespace config {

/**
 * Initialize config storage and load config.
 *
 * Checks local storage for persisted config first.
 * If found, loads it; otherwise loads embedded config.
 * Should be called during startup instead of parseConfig().
 */
void initConfig();

/**
 * Download a YAML config file from a URL and load it.
 *
 * This function:
 * 1. Downloads the file via HTTP/HTTPS
 * 2. Converts YAML to JSON
 * 3. Validates the config by parsing it
 * 4. Only if successful: persists to local storage and applies it
 *
 * The existing config is NOT cleared until the new one is validated.
 *
 * @param url The URL to download the config from
 * @return true on success, false on failure
 */
bool downloadAndLoadConfig(const std::string& url);

/**
 * Check if a persisted config exists in local storage.
 *
 * @return true if config file exists
 */
bool hasPersistedConfig();

/**
 * Delete the persisted config from local storage.
 * Next boot will use embedded config.
 */
void deletePersistedConfig();

} // namespace config
