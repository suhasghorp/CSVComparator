#include "row.h"

bool Row::operator==(const Row& other) const {
    if (columns.size() != other.columns.size()) return false;

    for (size_t i = 0; i < columns.size(); ++i) {
        if (!compareValues(columns[i], other.columns[i])) {
            return false;
        }
    }
    return true;
}

bool Row::compareValues(std::string_view v1, std::string_view v2) {
    try {
        size_t pos1, pos2;
        std::string s1(v1);
        std::string s2(v2);
        double d1 = std::stod(s1, &pos1);
        double d2 = std::stod(s2, &pos2);

        if (pos1 == v1.length() && pos2 == v2.length()) {
            double rounded1 = std::round(d1 * 10000.0) / 10000.0;
            double rounded2 = std::round(d2 * 10000.0) / 10000.0;
            return std::abs(rounded1 - rounded2) < 1e-9;
        }
    }
    catch (...) {
    }

    return v1 == v2;
}

size_t Row::Hash::operator()(const Row& row) const {
    size_t hash = 0;
    for (const auto& col : row.columns) {
        std::string normalized = normalizeForHash(col);
        hash ^= std::hash<std::string>{}(normalized)+0x9e3779b9 + (hash << 6) + (hash >> 2);
    }
    return hash;
}

std::string Row::Hash::normalizeForHash(std::string_view value) {
    try {
        size_t pos;
        std::string s(value);
        double d = std::stod(s, &pos);
        if (pos == value.length()) {
            double rounded = std::round(d * 10000.0) / 10000.0;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(4) << rounded;
            return oss.str();
        }
    }
    catch (...) {
    }
    return std::string(value);
}