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

bool hasPersistedConfig() {
    if (!SPIFFS.begin(true)) {
        omote_log_e("Failed to mount SPIFFS");
        return false;
    }
    return SPIFFS.exists(CONFIG_FILE_PATH);
}

void deletePersistedConfig() {
    if (!SPIFFS.begin(true)) {
        omote_log_e("Failed to mount SPIFFS");
        return;
    }
    if (SPIFFS.exists(CONFIG_FILE_PATH)) {
        SPIFFS.remove(CONFIG_FILE_PATH);
        omote_log_i("Persisted config deleted");
    }
}

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

    omote_log_i("Config loaded from storage (%d bytes)", json.length());
    return true;
}

static bool validateJson(const std::string& json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        omote_log_e("JSON parse error: %s", error.c_str());
        return false;
    }

    // Basic validation - check required top-level keys
    JsonObject root = doc.as<JsonObject>();
    if (!root.containsKey("devices") || !root.containsKey("scenes")) {
        omote_log_e("Invalid config: missing 'devices' or 'scenes'");
        return false;
    }

    return true;
}

void initConfig() {
    std::string json;

    // Try to load from storage first
    if (loadConfigFromStorage(json)) {
        omote_log_i("Using persisted config from storage");
        if (validateJson(json)) {
            parseConfig(json.c_str());
            return;
        }
        omote_log_w("Persisted config invalid, falling back to embedded");
    }

    // Fall back to embedded config
    omote_log_i("Using embedded config");
    parseConfig(nullptr);
}

bool downloadAndLoadConfig(const std::string& url) {
    if (!WiFi.isConnected()) {
        omote_log_e("WiFi not connected, cannot download config");
        return false;
    }

    omote_log_i("Downloading config from: %s", url.c_str());

    HTTPClient http;
    http.begin(url.c_str());
    http.setTimeout(10000);

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        omote_log_e("HTTP GET failed, code: %d", httpCode);
        http.end();
        return false;
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
        json = yamlToJson(content);
        if (json.empty()) {
            omote_log_e("YAML to JSON conversion failed");
            return false;
        }
    }

    // Validate the new config before applying
    if (!validateJson(json)) {
        omote_log_e("Downloaded config validation failed");
        return false;
    }

    // Save to storage
    if (!saveConfigToStorage(json)) {
        omote_log_e("Failed to persist config");
        return false;
    }

    // Reload config (clear + parse + re-register defaults)
    reload(json.c_str());

    omote_log_i("Config downloaded and applied successfully");
    return true;
}

} // namespace config

#endif // ENABLE_WIFI_AND_MQTT
