#pragma once

#include <string>
#include <vector>
#include <applicationInternal/omote_log.h>

namespace config {

/**
 * Represents an error or warning from config loading.
 */
struct ConfigError {
    enum Stage {
        YAML_PARSE,        // Error during YAML to JSON conversion
        JSON_PARSE,        // Error during JSON deserialization
        CONFIG_VALIDATION  // Error during config validation/parsing
    };

    Stage stage;
    std::string message;
    int line;  // 1-based line number, 0 if not applicable

    ConfigError(Stage s, const std::string& msg, int l = 0)
        : stage(s), message(msg), line(l) {}
};

/**
 * Result of config loading operation.
 */
struct ConfigLoadResult {
    bool success;
    std::string json;                    // JSON output (for YAML conversion)
    std::vector<ConfigError> errors;     // Fatal errors
    std::vector<ConfigError> warnings;   // Non-fatal warnings

    ConfigLoadResult() : success(true) {}

    void addError(ConfigError::Stage stage, const std::string& msg, int line = 0) {
        if (line > 0) {
            omote_log_e("Config error (line %d): %s", line, msg.c_str());
        } else {
            omote_log_e("Config error: %s", msg.c_str());
        }
        errors.emplace_back(stage, msg, line);
        success = false;
    }

    void addWarning(ConfigError::Stage stage, const std::string& msg, int line = 0) {
        if (line > 0) {
            omote_log_w("Config warning (line %d): %s", line, msg.c_str());
        } else {
            omote_log_w("Config warning: %s", msg.c_str());
        }
        warnings.emplace_back(stage, msg, line);
    }

    bool hasErrors() const { return !errors.empty(); }
    bool hasWarnings() const { return !warnings.empty(); }
};

/**
 * Convert a YAML string to JSON using ArduinoYaml library.
 *
 * @param yaml The YAML string to convert
 * @return ConfigLoadResult containing JSON string or error info
 */
ConfigLoadResult yamlToJson(const std::string& yaml);

} // namespace config
