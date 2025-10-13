#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <cmath>
#include <sstream>
#include <iomanip>

struct Row {
    std::vector<std::string> columns;

    bool operator==(const Row& other) const;
    static bool compareValues(std::string_view v1, std::string_view v2);

    struct Hash {
        size_t operator()(const Row& row) const;
    private:
        static std::string normalizeForHash(std::string_view value);
    };
};