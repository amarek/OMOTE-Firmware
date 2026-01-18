#include <HTTPClient.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <applicationInternal/config/configDownloader.h>
#include <applicationInternal/config/yamlToJson.h>
#include <applicationInternal/config/registry.h>
#include <applicationInternal/config/parser.h>
#include <applicationInternal/omote_log.h>

#if (ENABLE_WIFI_AND_MQTT == 1)

namespace config {

static const char* CONFIG_FILE_PATH = "/config.json";

static bool saveConfigToStorage(const std::string& json) {
    if (!SPIFFS.begin(true)) {
        omote_log_e("Failed to mount SPIFFS");
        return false;
    }

    File file = SPIFFS.open(CONFIG_FILE_PATH, FILE_WRITE);
    if (!file) {
        omote_log_e("Failed to open config file for writing");
        return false;
    }

    size_t written = file.print(json.c_str());
    file.close();

    if (written != json.length()) {
        omote_log_e("Failed to write complete config file");
        return false;
    }

    omote_log_i("Config saved to storage (%d bytes)", written);
    return true;
}

static bool loadConfigFromStorage(std::string& json) {
    if (!SPIFFS.begin(true)) {
        omote_log_e("Failed to mount SPIFFS");
        return false;
    }

    if (!SPIFFS.exists(CONFIG_FILE_PATH)) {
        return false;
    }

    File file = SPIFFS.open(CONFIG_FILE_PATH, FILE_READ);
    if (!file) {
        omote_log_e("Failed to open config file for reading");
        return false;
    }

    json = file.readString().c_str();
    file.close();

    omote_log_w("Config loaded from storage (%d bytes)", json.length());
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

    if (!WiFi.isConnected()) {
        result.addError(ConfigError::CONFIG_VALIDATION, "WiFi not connected");
        return result;
    }

    omote_log_w("Downloading config from: %s", url.c_str());

    HTTPClient http;
    http.begin(url.c_str());
    http.setTimeout(10000);

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        result.addError(ConfigError::CONFIG_VALIDATION,
                       "HTTP GET failed, code: " + std::to_string(httpCode));
        http.end();
        return result;
    }

    String payload = http.getString();
    http.end();

    omote_log_i("Downloaded %d bytes", payload.length());

    std::string content(payload.c_str());
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

    // Validate the new config before applying
    if (!validateJson(json, result)) {
        return result;
    }

    // Save to storage
    if (!saveConfigToStorage(json)) {
        result.addError(ConfigError::CONFIG_VALIDATION, "Failed to persist config");
        return result;
    }

    // Reload config (clear + parse + re-register defaults)
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
        omote_log_w("Config downloaded and applied successfully");
    }
    return result;
}

ConfigLoadResult clearPersistedConfig() {
    ConfigLoadResult result;

    if (!SPIFFS.begin(true)) {
        result.addError(ConfigError::CONFIG_VALIDATION, "Failed to mount SPIFFS");
        return result;
    }

    if (SPIFFS.exists(CONFIG_FILE_PATH)) {
        if (!SPIFFS.remove(CONFIG_FILE_PATH)) {
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
