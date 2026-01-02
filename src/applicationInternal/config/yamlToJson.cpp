#include "yamlToJson.h"
#include <vector>
#include <sstream>

namespace config {

namespace {

struct YamlLine {
    int indent;
    std::string key;
    std::string value;
    bool isList;
    int lineNum;
};

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string escapeJson(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }
    return result;
}

std::string unquote(const std::string& s) {
    if (s.length() >= 2) {
        if ((s[0] == '"' && s.back() == '"') || (s[0] == '\'' && s.back() == '\'')) {
            return s.substr(1, s.length() - 2);
        }
    }
    return s;
}

bool isQuoted(const std::string& s) {
    if (s.length() >= 2) {
        return (s[0] == '"' && s.back() == '"') || (s[0] == '\'' && s.back() == '\'');
    }
    return false;
}

std::string valueToJson(const std::string& val) {
    // If value was quoted in YAML, always treat as string
    bool wasQuoted = isQuoted(val);
    std::string v = unquote(val);

    if (!wasQuoted) {
        // Check for boolean
        if (v == "true" || v == "false") return v;

        // Check for null
        if (v == "null" || v == "~") return "null";

        // Check for number
        if (!v.empty()) {
            bool isNumber = true;
            bool hasDecimal = false;
            for (size_t i = 0; i < v.length(); i++) {
                char c = v[i];
                if (i == 0 && (c == '-' || c == '+')) continue;
                if (c == '.' && !hasDecimal) { hasDecimal = true; continue; }
                if (!isdigit(c)) { isNumber = false; break; }
            }
            if (isNumber && !v.empty()) {
                // Don't treat strings starting with 0 as numbers (except "0" itself)
                if (v.length() == 1 || v[0] != '0' || hasDecimal) {
                    return v;
                }
            }
        }
    }

    return "\"" + escapeJson(v) + "\"";
}

std::vector<YamlLine> parseYaml(const std::string& yaml) {
    std::vector<YamlLine> lines;
    std::istringstream stream(yaml);
    std::string rawLine;
    int lineNum = 0;

    while (std::getline(stream, rawLine)) {
        lineNum++;

        // Count indent
        int indent = 0;
        for (char c : rawLine) {
            if (c == ' ') indent++;
            else break;
        }

        std::string line = trim(rawLine);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;

        YamlLine yl = {indent, "", "", false, lineNum};

        // Check for list item
        if (line[0] == '-') {
            yl.isList = true;
            line = trim(line.substr(1));
            if (line.empty()) {
                lines.push_back(yl);
                continue;
            }
        }

        // Find key: value
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            yl.key = trim(line.substr(0, colonPos));
            yl.value = trim(line.substr(colonPos + 1));
        } else {
            yl.value = line;
        }

        lines.push_back(yl);
    }

    return lines;
}

// Recursive converter
size_t convertToJson(const std::vector<YamlLine>& lines, size_t idx, int parentIndent,
                     bool inArray, std::string& out, ConfigLoadResult& result);

size_t convertObject(const std::vector<YamlLine>& lines, size_t idx, int baseIndent,
                     std::string& out, ConfigLoadResult& result) {
    out += "{";
    bool first = true;

    while (idx < lines.size() && lines[idx].indent > baseIndent) {
        const YamlLine& line = lines[idx];

        // Skip if this is a list item (handled by convertArray)
        if (line.isList) break;

        if (!first) out += ",";
        first = false;

        out += "\"" + escapeJson(line.key) + "\":";

        if (line.value.empty()) {
            // Check what comes next
            bool hasChildren = false;
            bool childIsList = false;
            if (idx + 1 < lines.size()) {
                const YamlLine& next = lines[idx + 1];
                // Child if: higher indent, OR same indent but it's a list item (YAML allows this)
                if (next.indent > line.indent || (next.indent == line.indent && next.isList)) {
                    hasChildren = true;
                    childIsList = next.isList;
                }
            }

            if (hasChildren) {
                const YamlLine& next = lines[idx + 1];
                if (childIsList) {
                    // If list is at same indent as key, use lower baseIndent so array loop processes it
                    int arrayBase = (next.indent == line.indent) ? line.indent - 1 : line.indent;
                    idx = convertToJson(lines, idx + 1, arrayBase, true, out, result);
                } else {
                    idx = convertToJson(lines, idx + 1, line.indent, false, out, result);
                }
            } else {
                out += "\"\"";
                idx++;
            }
        } else {
            out += valueToJson(line.value);
            idx++;
        }
    }

    out += "}";
    return idx;
}

size_t convertArray(const std::vector<YamlLine>& lines, size_t idx, int baseIndent,
                    std::string& out, ConfigLoadResult& result) {
    out += "[";
    bool first = true;

    while (idx < lines.size() && lines[idx].indent > baseIndent && lines[idx].isList) {
        const YamlLine& line = lines[idx];

        if (!first) out += ",";
        first = false;

        if (line.key.empty() && !line.value.empty()) {
            // Simple list value: - value
            out += valueToJson(line.value);
            idx++;
        } else if (line.key.empty() && line.value.empty()) {
            // List item with nested content: -\n  key: value
            if (idx + 1 < lines.size() && lines[idx + 1].indent > line.indent) {
                if (lines[idx + 1].isList) {
                    idx = convertToJson(lines, idx + 1, line.indent, true, out, result);
                } else {
                    idx = convertToJson(lines, idx + 1, line.indent, false, out, result);
                }
            } else {
                out += "null";
                idx++;
            }
        } else {
            // List item starting with key:value - this is an object
            // - key: value
            //   key2: value2
            out += "{";
            bool objFirst = true;

            // First property from the list line itself
            out += "\"" + escapeJson(line.key) + "\":";
            if (line.value.empty()) {
                // Nested structure under this key
                if (idx + 1 < lines.size() && lines[idx + 1].indent > line.indent) {
                    if (lines[idx + 1].isList) {
                        idx = convertToJson(lines, idx + 1, line.indent, true, out, result);
                    } else {
                        idx = convertToJson(lines, idx + 1, line.indent, false, out, result);
                    }
                } else {
                    out += "\"\"";
                    idx++;
                }
            } else {
                out += valueToJson(line.value);
                idx++;
            }
            objFirst = false;

            // Continue with sibling properties at same effective indent
            int objIndent = line.indent + 2; // Properties are indented relative to the dash
            while (idx < lines.size() && lines[idx].indent >= objIndent && !lines[idx].isList) {
                const YamlLine& prop = lines[idx];
                if (prop.indent > objIndent) {
                    // This is a child, not sibling - should have been handled above
                    break;
                }

                if (!objFirst) out += ",";
                objFirst = false;

                out += "\"" + escapeJson(prop.key) + "\":";
                if (prop.value.empty()) {
                    if (idx + 1 < lines.size() && lines[idx + 1].indent > prop.indent) {
                        if (lines[idx + 1].isList) {
                            idx = convertToJson(lines, idx + 1, prop.indent, true, out, result);
                        } else {
                            idx = convertToJson(lines, idx + 1, prop.indent, false, out, result);
                        }
                    } else {
                        out += "\"\"";
                        idx++;
                    }
                } else {
                    out += valueToJson(prop.value);
                    idx++;
                }
            }

            out += "}";
        }
    }

    out += "]";
    return idx;
}

size_t convertToJson(const std::vector<YamlLine>& lines, size_t idx, int parentIndent,
                     bool inArray, std::string& out, ConfigLoadResult& result) {
    if (inArray) {
        return convertArray(lines, idx, parentIndent, out, result);
    } else {
        return convertObject(lines, idx, parentIndent, out, result);
    }
}

} // anonymous namespace

ConfigLoadResult yamlToJson(const std::string& yaml) {
    ConfigLoadResult result;

    std::vector<YamlLine> lines = parseYaml(yaml);

    if (lines.empty()) {
        result.json = "{}";
        return result;
    }

    std::string json;

    // Start with root object
    if (lines[0].isList) {
        convertArray(lines, 0, -1, json, result);
    } else {
        convertObject(lines, 0, -1, json, result);
    }

    result.json = json;
    return result;
}

} // namespace config
