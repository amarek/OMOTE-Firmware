#pragma once

#include <string>

namespace config {

/**
 * Convert a simple YAML string to JSON.
 *
 * Supports the subset of YAML used in config.yml:
 * - Indentation-based nesting (2 spaces per level)
 * - Key: value pairs
 * - Lists with - prefix
 * - Simple string values (quoted or unquoted)
 * - Comments starting with #
 *
 * Does NOT support:
 * - Anchors and references
 * - Multi-line strings
 * - Complex types
 *
 * @param yaml The YAML string to convert
 * @return JSON string, or empty string on error
 */
std::string yamlToJson(const std::string& yaml);

} // namespace config
