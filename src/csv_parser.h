#pragma once

#include "row.h"
#include <string>
#include <string_view>
#include <vector>

class CSVParser {
public:
    static std::vector<std::string> parseCSVLine(std::string_view line);
    static Row parseCSVRow(std::string_view line);
};