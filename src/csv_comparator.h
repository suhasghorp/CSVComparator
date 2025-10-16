#pragma once

#include "row.h"
#include <string>
#include <unordered_set>
#include <vector>

// Tracy profiler integration
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedN(name)
#define ZoneName(name, size)
#define TracyPlot(name, value)
#define FrameMark
#define FrameMarkNamed(name)
#endif

class CSVComparator {
public:
    CSVComparator() = default;
    ~CSVComparator() = default;

    struct ComparisonResult {
        bool filesMatch;
        size_t file1RowCount;
        size_t file2RowCount;
        std::vector<Row> onlyInFile1;
        std::vector<Row> onlyInFile2;
    };

    ComparisonResult compare(const std::string& file1, const std::string& file2);
    void writeRowsToCSV(const std::string& filename, const std::vector<Row>& rows);

private:
    size_t countRows(const std::string& filename);
    std::unordered_set<Row, Row::Hash> readCSV(const std::string& filename);
};