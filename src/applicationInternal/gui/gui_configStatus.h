#pragma once

#include <string>
#include <applicationInternal/config/yamlToJson.h>

/**
 * Show a modal dialog for config download status.
 * Call this before starting a download to show "Downloading..."
 */
void configStatus_showDownloading();

/**
 * Update the modal to show the result of the download/parse operation.
 * Shows success message, or errors/warnings with line numbers.
 *
 * @param result The ConfigLoadResult from downloadAndLoadConfig
 */
void configStatus_showResult(const config::ConfigLoadResult& result);

/**
 * Close the config status modal if open.
 */
void configStatus_close();
