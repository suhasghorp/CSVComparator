#include "file_comparator.h"
#include "csv_parser.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <xlnt/xlnt.hpp>
#include <sstream>
#include <iomanip>

// ============ CSV FUNCTIONS (EXISTING) ============

size_t FileComparator::countRowsCSV(const std::string& filename) {
    ZoneScoped;
    ZoneName("Count Rows CSV", 14);

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
    TracyPlot("Row Count CSV", static_cast<int64_t>(count));
#endif

    return count;
}

std::unordered_set<Row, Row::Hash> FileComparator::readCSV(const std::string& filename) {
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

// ============ XLSX FUNCTIONS (NEW) ============

size_t FileComparator::countRowsXLSX(const std::string& filename) {
    ZoneScoped;
    ZoneName("Count Rows XLSX", 15);

    try {
        xlnt::workbook wb;
        wb.load(filename);

        if (wb.sheet_count() == 0) {
            throw std::runtime_error("No sheets found in XLSX file");
        }

        auto ws = wb.active_sheet();
        size_t count = 0;
        for (auto row : ws.rows()) {
            ++count;
        }

#ifdef TRACY_ENABLE
        TracyPlot("Row Count XLSX", static_cast<int64_t>(count));
#endif

        return count;

    }
    catch (const xlnt::exception& e) {
        throw std::runtime_error("Error reading XLSX file: " + std::string(e.what()));
    }
}

std::unordered_set<Row, Row::Hash> FileComparator::readXLSX(const std::string& filename) {
    ZoneScoped;
    ZoneName("Read XLSX", 10);

    std::unordered_set<Row, Row::Hash> rows;

    try {
        xlnt::workbook wb;

        {
            ZoneScoped;
            ZoneName("Load XLSX Workbook", 18);
            wb.load(filename);
        }

        if (wb.sheet_count() == 0) {
            throw std::runtime_error("No sheets found in XLSX file");
        }

        auto ws = wb.active_sheet();

        {
            ZoneScoped;
            ZoneName("Parse XLSX Rows", 15);

            for (auto xlnt_row : ws.rows()) {
                Row row;

                for (auto cell : xlnt_row) {
                    std::string value;

                    if (cell.has_value()) {
                        switch (cell.data_type()) {
                        case xlnt::cell::type::number: {
                            double d = cell.value<double>();

                            // Check if it's actually an integer
                            if (d == std::floor(d) && std::abs(d) < 1e15) {
                                // Integer - no decimal point
                                value = std::to_string(static_cast<long long>(d));
                            }
                            else {
                                // Floating point - preserve precision
                                std::ostringstream oss;
                                oss << std::fixed << std::setprecision(10) << d;
                                value = oss.str();

                                // Remove trailing zeros
                                value.erase(value.find_last_not_of('0') + 1);
                                if (value.back() == '.') {
                                    value.pop_back();
                                }
                            }
                            break;
                        }

                        case xlnt::cell::type::shared_string:
                        case xlnt::cell::type::inline_string:
                            value = cell.value<std::string>();
                            break;

                        case xlnt::cell::type::boolean:
                            value = cell.value<bool>() ? "true" : "false";
                            break;

                        case xlnt::cell::type::date:
                            // Format date as string
                            value = cell.to_string();
                            break;

                        case xlnt::cell::type::formula_string:
                            // Get calculated value, not formula
                            value = cell.to_string();
                            break;

                        default:
                            value = cell.to_string();
                            break;
                        }
                    }
                    else {
                        // Empty cell
                        value = "";
                    }

                    row.columns.push_back(value);
                }

                rows.insert(row);
            }
        }

        return rows;

    }
    catch (const xlnt::exception& e) {
        throw std::runtime_error("Error reading XLSX file: " + std::string(e.what()));
    }
}

// ============ AUTO-DISPATCH FUNCTIONS (NEW) ============

size_t FileComparator::countRowsAuto(const std::string& filename) {
    FileType type = FileTypeDetector::detect(filename);

    switch (type) {
    case FileType::CSV:
        return countRowsCSV(filename);
    case FileType::XLSX:
        return countRowsXLSX(filename);
    default:
        throw std::runtime_error("Unsupported file type: " + filename);
    }
}

std::unordered_set<Row, Row::Hash> FileComparator::readFileAuto(const std::string& filename) {
    FileType type = FileTypeDetector::detect(filename);

    switch (type) {
    case FileType::CSV:
        return readCSV(filename);
    case FileType::XLSX:
        return readXLSX(filename);
    default:
        throw std::runtime_error("Unsupported file type: " + filename);
    }
}

// ============ COMPARISON AND OUTPUT (UPDATED) ============

void FileComparator::writeRowsToCSV(const std::string& filename, const std::vector<Row>& rows) {
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

FileComparator::ComparisonResult FileComparator::compare(
    const std::string& file1,
    const std::string& file2) {

    ZoneScoped;
    ZoneName("File Compare", 12);

    std::cout << "Comparing files:" << std::endl;
    std::cout << "  File 1: " << file1 << std::endl;
    std::cout << "  File 2: " << file2 << std::endl;

    // Detect and report file types
    FileType type1 = FileTypeDetector::detect(file1);
    FileType type2 = FileTypeDetector::detect(file2);

    std::cout << "  File 1 type: " << FileTypeDetector::toString(type1) << std::endl;
    std::cout << "  File 2 type: " << FileTypeDetector::toString(type2) << std::endl;
    std::cout << std::endl;

    // Count rows for reporting
    std::cout << "Counting rows..." << std::endl;
    size_t count1 = countRowsAuto(file1);
    size_t count2 = countRowsAuto(file2);
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
        rows1 = readFileAuto(file1);
#ifdef TRACY_ENABLE
        TracyPlot("File 1 Rows", static_cast<int64_t>(rows1.size()));
#endif
    }

    {
        ZoneScoped;
        ZoneName("Read File 2", 11);
        rows2 = readFileAuto(file2);
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