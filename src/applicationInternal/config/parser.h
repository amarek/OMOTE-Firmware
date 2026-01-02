#pragma once
#include <applicationInternal/config/yamlToJson.h>

// Parse config and collect errors/warnings
// If json is nullptr, uses embedded config
// Returns ConfigLoadResult with success status and any errors/warnings
config::ConfigLoadResult parseConfig(const char* json = nullptr);
