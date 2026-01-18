#include <applicationInternal/config/configDownloader.h>
#include <applicationInternal/config/yamlToJson.h>
#include <applicationInternal/config/registry.h>
#include <applicationInternal/config/parser.h>
#include <applicationInternal/omote_log.h>
#include <ArduinoJson.h>
#include <fstream>
#include <sstream>
#include <cstdio>

#if (ENABLE_WIFI_AND_MQTT == 1)

namespace config {

static const char* CONFIG_FILE_PATH = "config.json";

static bool saveConfigToStorage(const std::string& json) {
    std::ofstream file(CONFIG_FILE_PATH);
    if (!file.is_open()) {
        omote_log_e("Failed to open config file for writing");
        return false;
    }

    file << json;
    file.close();

    if (file.fail()) {
        omote_log_e("Failed to write complete config file");
        return false;
    }

    omote_log_i("Config saved to storage (%d bytes)", json.length());
    return true;
}

static bool loadConfigFromStorage(std::string& json) {
    std::ifstream file(CONFIG_FILE_PATH);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    json = buffer.str();
    file.close();

    omote_log_i("Config loaded from storage (%d bytes)", json.length());
    return true;
}

static bool validateJson(const std::string& json, ConfigLoadResult& result) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        result.addError(ConfigError::JSON_PARSE,
                       "JSON parse error: " + std::string(error.c_str()));
        return false;
    }

    // Basic validation - check required top-level keys
    JsonObject root = doc.as<JsonObject>();
    if (!root["devices"].is<JsonObject>() || !root["scenes"].is<JsonObject>()) {
        result.addError(ConfigError::CONFIG_VALIDATION,
                       "Invalid config: missing 'devices' or 'scenes'");
        return false;
    }

    return true;
}

void initConfig() {
    std::string json;

    // Try to load from storage first
    if (loadConfigFromStorage(json)) {
        omote_log_i("Using persisted config from storage");
        ConfigLoadResult result;
        if (validateJson(json, result)) {
            parseConfig(json.c_str());
            return;
        }
        omote_log_w("Persisted config invalid, falling back to embedded");
    }

    // Fall back to embedded config
    omote_log_i("Using embedded config");
    parseConfig(nullptr);
}

ConfigLoadResult downloadAndLoadConfig(const std::string& url) {
    ConfigLoadResult result;

    // HTTP download not supported in emulator - load from local file instead
    const char* localFile = "download.yml";
    omote_log_i("Ignoring URL '%s', loading from local file: %s", url.c_str(), localFile);

    std::ifstream file(localFile);
    if (!file.is_open()) {
        result.addError(ConfigError::CONFIG_VALIDATION,
                       std::string("Failed to open file: ") + localFile);
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    omote_log_i("Read %d bytes from %s", content.length(), localFile);

    std::string json;

    // Check if it's YAML or JSON
    size_t start = content.find_first_not_of(" \t\r\n");
    if (start != std::string::npos && content[start] == '{') {
        // Already JSON
        json = content;
    } else {
        // Convert YAML to JSON
        omote_log_i("Converting YAML to JSON");
        ConfigLoadResult yamlResult = yamlToJson(content);
        if (!yamlResult.success || yamlResult.json.empty()) {
            // Copy YAML errors to our result
            for (const auto& err : yamlResult.errors) {
                result.errors.push_back(err);
            }
            result.success = false;
            if (result.errors.empty()) {
                result.addError(ConfigError::YAML_PARSE, "YAML to JSON conversion failed");
            }
            return result;
        }
        json = yamlResult.json;
    }

    omote_log_i("Validating JSON: %s", json.c_str());    
    // Validate the new config before applying
    if (!validateJson(json, result)) {
        return result;
    }

    omote_log_i("Saving JSON");        
    // Save to storage
    if (!saveConfigToStorage(json)) {
        result.addError(ConfigError::CONFIG_VALIDATION, "Failed to persist config");
        return result;
    }


    omote_log_i("Re-loading");            
    ConfigLoadResult reloadResult = reload(json.c_str());

    // Copy any warnings/errors from reload
    for (const auto& err : reloadResult.errors) {
        result.errors.push_back(err);
    }
    for (const auto& warn : reloadResult.warnings) {
        result.warnings.push_back(warn);
    }
    if (!reloadResult.success) {
        result.success = false;
    }

    if (result.success) {
        omote_log_i("Config loaded and applied successfully");
    }
    return result;
}

ConfigLoadResult clearPersistedConfig() {
    ConfigLoadResult result;

    // Check if file exists and delete it
    std::ifstream checkFile(CONFIG_FILE_PATH);
    if (checkFile.good()) {
        checkFile.close();
        if (std::remove(CONFIG_FILE_PATH) != 0) {
            result.addError(ConfigError::CONFIG_VALIDATION, "Failed to delete config file");
            return result;
        }
        omote_log_w("Persisted config deleted");
    } else {
        omote_log_i("No persisted config to delete");
    }

    // Reload embedded config
    result = reload(nullptr);

    if (result.success) {
        omote_log_w("Embedded config loaded successfully");
    }
    return result;
}

} // namespace config

#endif // ENABLE_WIFI_AND_MQTT
