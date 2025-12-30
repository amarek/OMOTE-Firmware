#include "yamlToJson.h"
#include <vector>
#include <sstream>
#include <cstring>
#include <applicationInternal/omote_log.h>

namespace config {

namespace {

struct Line {
    int indent;
    std::string key;
    std::string value;
    bool isList;
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
        if ((s[0] == '"' && s[s.length()-1] == '"') ||
            (s[0] == '\'' && s[s.length()-1] == '\'')) {
            return s.substr(1, s.length() - 2);
        }
    }
    return s;
}

int countIndent(const std::string& line) {
    int count = 0;
    for (char c : line) {
        if (c == ' ') count++;
        else break;
    }
    return count;
}

Line parseLine(const std::string& rawLine) {
    Line result = {0, "", "", false};

    // Get indent
    result.indent = countIndent(rawLine);

    std::string line = trim(rawLine);

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
        result.key = "";
        return result;
    }

    // Check for list item
    if (line[0] == '-') {
        result.isList = true;
        line = trim(line.substr(1));
        if (line.empty()) {
            return result;
        }
    }

    // Find key: value separator
    size_t colonPos = line.find(':');
    if (colonPos != std::string::npos) {
        result.key = trim(line.substr(0, colonPos));
        std::string afterColon = line.substr(colonPos + 1);
        result.value = trim(afterColon);
    } else {
        // No colon - entire line is the value (for list items)
        result.value = line;
    }

    return result;
}

} // anonymous namespace

std::string yamlToJson(const std::string& yaml) {
    std::vector<Line> lines;
    std::istringstream stream(yaml);
    std::string rawLine;

    // Parse all lines
    while (std::getline(stream, rawLine)) {
        Line line = parseLine(rawLine);
        if (!line.key.empty() || line.isList || !line.value.empty()) {
            lines.push_back(line);
        }
    }

    if (lines.empty()) {
        return "{}";
    }

    std::string json;
    std::vector<int> indentStack;
    std::vector<bool> isArrayStack;
    std::vector<bool> needCommaStack;

    indentStack.push_back(-1);
    isArrayStack.push_back(false);
    needCommaStack.push_back(false);

    json += "{";

    for (size_t i = 0; i < lines.size(); i++) {
        Line& line = lines[i];

        // Close containers that are at higher indent levels
        while (indentStack.size() > 1 && line.indent <= indentStack.back()) {
            if (isArrayStack.back()) {
                json += "]";
            } else {
                json += "}";
            }
            indentStack.pop_back();
            isArrayStack.pop_back();
            needCommaStack.pop_back();
        }

        // Add comma if needed
        if (needCommaStack.back()) {
            json += ",";
        }

        // Check if next line is a child (higher indent or list)
        bool hasChildren = false;
        bool childIsList = false;
        if (i + 1 < lines.size()) {
            Line& next = lines[i + 1];
            if (next.indent > line.indent) {
                hasChildren = true;
                childIsList = next.isList;
            }
        }

        if (line.isList) {
            // We're inside a list
            if (!isArrayStack.back()) {
                // Start of array (shouldn't normally happen here)
            }

            if (line.key.empty() && !line.value.empty()) {
                // Simple list value
                json += "\"" + escapeJson(unquote(line.value)) + "\"";
            } else if (!line.key.empty()) {
                // List item is an object
                if (hasChildren || !line.value.empty()) {
                    json += "{\"" + escapeJson(line.key) + "\":";
                    if (line.value.empty()) {
                        // Value is a nested structure
                        if (childIsList) {
                            json += "[";
                            indentStack.push_back(line.indent + 2);
                            isArrayStack.push_back(true);
                            needCommaStack.push_back(false);
                        } else {
                            json += "{";
                            indentStack.push_back(line.indent + 2);
                            isArrayStack.push_back(false);
                            needCommaStack.push_back(false);
                        }
                    } else {
                        json += "\"" + escapeJson(unquote(line.value)) + "\"}";
                    }
                } else {
                    json += "{\"" + escapeJson(line.key) + "\":\"\"}";
                }
            }
        } else {
            // Regular key: value
            json += "\"" + escapeJson(line.key) + "\":";

            if (line.value.empty() && hasChildren) {
                // Value is a nested structure
                if (childIsList) {
                    json += "[";
                    indentStack.push_back(line.indent);
                    isArrayStack.push_back(true);
                    needCommaStack.push_back(false);
                } else {
                    json += "{";
                    indentStack.push_back(line.indent);
                    isArrayStack.push_back(false);
                    needCommaStack.push_back(false);
                }
            } else if (line.value.empty()) {
                json += "\"\"";
            } else {
                // Simple value - try to detect if it's a number
                std::string val = unquote(line.value);
                bool isNumber = !val.empty();
                bool hasDecimal = false;
                for (size_t j = 0; j < val.length(); j++) {
                    char c = val[j];
                    if (j == 0 && c == '-') continue;
                    if (c == '.' && !hasDecimal) { hasDecimal = true; continue; }
                    if (!isdigit(c)) { isNumber = false; break; }
                }

                if (isNumber && !val.empty() && val[0] != '0') {
                    json += val;
                } else {
                    json += "\"" + escapeJson(val) + "\"";
                }
            }
        }

        needCommaStack.back() = true;
    }

    // Close any remaining open containers
    while (indentStack.size() > 1) {
        if (isArrayStack.back()) {
            json += "]";
        } else {
            json += "}";
        }
        indentStack.pop_back();
        isArrayStack.pop_back();
        needCommaStack.pop_back();
    }

    json += "}";

    return json;
}

} // namespace config
