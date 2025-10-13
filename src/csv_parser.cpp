#include "csv_parser.h"

std::vector<std::string> CSVParser::parseCSVLine(std::string_view line) {
    std::vector<std::string> result;
    std::string current;
    bool inQuotes = false;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];

        if (c == '"') {
            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"') {
                current += '"';
                ++i;
            }
            else {
                inQuotes = !inQuotes;
            }
        }
        else if (c == ',' && !inQuotes) {
            result.push_back(current);
            current.clear();
        }
        else {
            current += c;
        }
    }
    result.push_back(current);

    for (auto& field : result) {
        size_t start = field.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            field.clear();
            continue;
        }
        size_t end = field.find_last_not_of(" \t\r\n");
        field = field.substr(start, end - start + 1);
    }

    return result;
}

Row CSVParser::parseCSVRow(std::string_view line) {
    Row row;
    row.columns = parseCSVLine(line);
    return row;
}