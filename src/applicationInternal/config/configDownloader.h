#pragma once

#include <string>
#include <applicationInternal/config/yamlToJson.h>

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
 * @return ConfigLoadResult with success status and any errors/warnings
 */
ConfigLoadResult downloadAndLoadConfig(const std::string& url);

/**
 * Clear the persisted config and reload embedded config.
 *
 * This function:
 * 1. Deletes the persisted config from storage
 * 2. Clears the current runtime config
 * 3. Loads the embedded config
 *
 * @return ConfigLoadResult with success status and any errors/warnings
 */
ConfigLoadResult clearPersistedConfig();

} // namespace config
