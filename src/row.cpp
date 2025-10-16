#include "row.h"
#include <wyhash.h>
#include <charconv>
#include <cstring>

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
    double d1, d2;

    auto [ptr1, ec1] = std::from_chars(v1.data(), v1.data() + v1.size(), d1);
    auto [ptr2, ec2] = std::from_chars(v2.data(), v2.data() + v2.size(), d2);

    // Check if entire strings were consumed and parsing succeeded
    if (ec1 == std::errc() && ptr1 == v1.data() + v1.size() &&
        ec2 == std::errc() && ptr2 == v2.data() + v2.size()) {
        // Round to 4 decimal places and compare
        double rounded1 = std::round(d1 * 10000.0) / 10000.0;
        double rounded2 = std::round(d2 * 10000.0) / 10000.0;
        return std::abs(rounded1 - rounded2) < 1e-9;
    }
    // String comparison (case-sensitive)
    return v1 == v2;
}

bool compareValues2(std::string_view v1, std::string_view v2) {
    // Try to parse as double
    try {
        size_t pos1, pos2;
        std::string s1(v1);
        std::string s2(v2);
        double d1 = std::stod(s1, &pos1);
        double d2 = std::stod(s2, &pos2);

        // Check if entire strings were consumed (they are valid numbers)
        if (pos1 == v1.length() && pos2 == v2.length()) {
            // Round to 4 decimal places and compare
            double rounded1 = std::round(d1 * 10000.0) / 10000.0;
            double rounded2 = std::round(d2 * 10000.0) / 10000.0;
            return std::abs(rounded1 - rounded2) < 1e-9;
        }
    }
    catch (...) {
        // Not a number, fall through to string comparison
    }

    // String comparison (case-sensitive)
    return v1 == v2;
}

//   OPTIMIZED: Using wyhash instead of std::hash
size_t Row::Hash::operator()(const Row& row) const {
    // Use wyhash with cumulative hashing
    uint64_t hash = 0;

    for (const auto& col : row.columns) {
        // Normalize value for hashing (handles decimal comparison)
        std::string normalized = normalizeForHash(col);

        // Hash the normalized value using wyhash
        // Previous hash is used as seed for next iteration
        hash = wyhash(normalized.data(), normalized.size(), hash, _wyp);

        // Mix in a null byte as delimiter to prevent concatenation issues
        // "ab" + "cd" should hash differently from "abc" + "d"
        const char delimiter = '\0';
        hash = wyhash(&delimiter, 1, hash, _wyp);
    }

    return static_cast<size_t>(hash);
}

std::string Row::Hash::normalizeForHash(std::string_view value) {
    try {
        size_t pos;
        std::string s(value);
        double d = std::stod(s, &pos);
        if (pos == value.length()) {
            // It's a valid number, normalize to 4 decimal places
            double rounded = std::round(d * 10000.0) / 10000.0;
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(4) << rounded;
            return oss.str();
        }
    }
    catch (...) {
        // Not a number
    }
    return std::string(value);
}