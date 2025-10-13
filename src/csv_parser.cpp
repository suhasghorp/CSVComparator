#include "csv_parser.h"
#include <cctype>

// ? OPTIMIZATION: Helper function for in-place trimming (single pass)
static inline void trimTrailingWhitespace(std::string& str) {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
        str.pop_back();
    }
}

std::vector<std::string> CSVParser::parseCSVLine(std::string_view line) {
    std::vector<std::string> result;

    // ? OPTIMIZATION 1: Pre-allocate vector capacity
    // Typical CSV has 10 columns, allocate 12 for safety margin
    result.reserve(12);

    std::string current;

    // ? OPTIMIZATION 2: Pre-allocate string capacity
    // Typical field is ~64 bytes, pre-allocate to avoid reallocations
    current.reserve(64);

    bool inQuotes = false;
    bool fieldWasQuoted = false;  // ? FIX: Track if this field had quotes
    bool hasContent = false;  // Track if field has non-whitespace

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];

        if (c == '"') {
            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"') {
                // Escaped quote inside quoted field
                current.push_back('"');
                hasContent = true;
                ++i;
            }
            else {
                // Toggle quote state
                inQuotes = !inQuotes;
                if (inQuotes) {
                    // ? FIX: Mark that this field is quoted
                    fieldWasQuoted = true;
                }
            }
        }
        else if (c == ',' && !inQuotes) {
            // Field delimiter found

            // ? FIX: Only trim if field was NOT quoted
            // Quoted fields preserve all whitespace
            if (!fieldWasQuoted) {
                trimTrailingWhitespace(current);
            }

            // ? OPTIMIZATION 4: Use move semantics to avoid copy
            result.emplace_back(std::move(current));

            // Prepare for next field
            current.clear();

            // ? OPTIMIZATION 5: Maintain string capacity after clear
            current.reserve(64);
            hasContent = false;
            fieldWasQuoted = false;  // ? FIX: Reset for next field
        }
        else {
            // ? OPTIMIZATION 6: Trim leading whitespace during parsing (ONLY if not quoted)
            // Skip leading whitespace (unless we're inside quotes or field was quoted)
            if (!hasContent && !fieldWasQuoted && !inQuotes && std::isspace(static_cast<unsigned char>(c))) {
                continue;  // Never added to string, no allocation
            }

            hasContent = true;
            current.push_back(c);
        }
    }

    // Handle last field
    // ? FIX: Only trim trailing whitespace if field was NOT quoted
    if (!fieldWasQuoted) {
        trimTrailingWhitespace(current);
    }
    result.emplace_back(std::move(current));

    return result;
}

Row CSVParser::parseCSVRow(std::string_view line) {
    Row row;
    row.columns = parseCSVLine(line);
    return row;
}