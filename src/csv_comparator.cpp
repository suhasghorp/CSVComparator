#include "csv_comparator.h"
#include "csv_parser.h"
#include <fstream>
#include <iostream>
#include <algorithm>

size_t CSVComparator::countRows(const std::string& filename) {
    ZoneScoped;
    ZoneName("Count Rows", 10);

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    size_t count = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            ++count;
        }
    }

#ifdef TRACY_ENABLE
    TracyPlot("Row Count", static_cast<int64_t>(count));
#endif

    return count;
}

std::unordered_set<Row, Row::Hash> CSVComparator::readCSV(const std::string& filename) {
    ZoneScoped;
    ZoneName("Read CSV", 8);

    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    std::unordered_set<Row, Row::Hash> rows;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        rows.insert(CSVParser::parseCSVRow(line));
    }

    return rows;
}

void CSVComparator::writeRowsToCSV(const std::string& filename, const std::vector<Row>& rows) {
    ZoneScoped;
    ZoneName("Write CSV Output", 16);

    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open output file: " + filename);
    }

    for (const auto& row : rows) {
        for (size_t i = 0; i < row.columns.size(); ++i) {
            if (i > 0) file << ",";

            const auto& col = row.columns[i];
            if (col.find(',') != std::string::npos ||
                col.find('"') != std::string::npos ||
                col.find('\n') != std::string::npos) {
                file << "\"";
                for (char c : col) {
                    if (c == '"') file << "\"\"";
                    else file << c;
                }
                file << "\"";
            }
            else {
                file << col;
            }
        }
        file << "\n";
    }
}

CSVComparator::ComparisonResult CSVComparator::compare(
    const std::string& file1,
    const std::string& file2) {

    ZoneScoped;
    ZoneName("CSV Compare", 11);

    std::cout << "Comparing files:" << std::endl;
    std::cout << "  File 1: " << file1 << std::endl;
    std::cout << "  File 2: " << file2 << std::endl;
    std::cout << std::endl;

    // Count rows for reporting
    std::cout << "Counting rows..." << std::endl;
    size_t count1 = countRows(file1);
    size_t count2 = countRows(file2);
    std::cout << "  File 1: " << count1 << " rows" << std::endl;
    std::cout << "  File 2: " << count2 << " rows" << std::endl;
    std::cout << std::endl;

    // Read both files
    std::cout << "Reading files..." << std::endl;
    std::unordered_set<Row, Row::Hash> rows1;
    std::unordered_set<Row, Row::Hash> rows2;

    {
        ZoneScoped;
        ZoneName("Read File 1", 11);
        rows1 = readCSV(file1);
#ifdef TRACY_ENABLE
        TracyPlot("File 1 Rows", static_cast<int64_t>(rows1.size()));
#endif
    }

    {
        ZoneScoped;
        ZoneName("Read File 2", 11);
        rows2 = readCSV(file2);
#ifdef TRACY_ENABLE
        TracyPlot("File 2 Rows", static_cast<int64_t>(rows2.size()));
#endif
    }

    // Build result
    ComparisonResult result;
    result.file1RowCount = rows1.size();
    result.file2RowCount = rows2.size();

    // Find differences
    std::cout << "Finding differences..." << std::endl;
    {
        ZoneScoped;
        ZoneName("Find Differences", 16);

        for (const auto& row : rows1) {
            if (rows2.find(row) == rows2.end()) {
                result.onlyInFile1.push_back(row);
            }
        }

        for (const auto& row : rows2) {
            if (rows1.find(row) == rows1.end()) {
                result.onlyInFile2.push_back(row);
            }
        }
    }

    result.filesMatch = result.onlyInFile1.empty() && result.onlyInFile2.empty();

#ifdef TRACY_ENABLE
    TracyPlot("Files Match", result.filesMatch ? 1 : 0);
    TracyPlot("Differences Found",
        static_cast<int64_t>(result.onlyInFile1.size() + result.onlyInFile2.size()));
#endif

    return result;
}