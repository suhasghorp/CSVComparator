#include "file_comparator.h"
#include <iostream>
#include <cstdio>
#include <sstream>

std::string formatRow(const Row& row) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < row.columns.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << row.columns[i] << "\"";
    }
    oss << "]";
    return oss.str();
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <file1> <file2>" << std::endl;
        std::cerr << std::endl;
        std::cerr << "File Comparator - High-performance file comparison" << std::endl;
        std::cerr << "Compares two CSV or XLSX files and reports differences." << std::endl;
        std::cerr << std::endl;
        std::cerr << "Supported formats:" << std::endl;
        std::cerr << "  - CSV  (.csv)" << std::endl;
        std::cerr << "  - XLSX (.xlsx)" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Features:" << std::endl;
        std::cerr << "  - Order-independent comparison" << std::endl;
        std::cerr << "  - Decimal numbers compared to 4 decimal places" << std::endl;
        std::cerr << "  - Mixed format comparison (CSV vs XLSX)" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Examples:" << std::endl;
        std::cerr << "  " << argv[0] << " data1.csv data2.csv" << std::endl;
        std::cerr << "  " << argv[0] << " report1.xlsx report2.xlsx" << std::endl;
        std::cerr << "  " << argv[0] << " export.csv backup.xlsx" << std::endl;
        return 1;
    }

    try {
        std::string file1 = argv[1];
        std::string file2 = argv[2];

        FileComparator comparator;
        auto result = comparator.compare(file1, file2);

        std::cout << std::endl;

        if (result.filesMatch) {
            std::cout << "FILES MATCH" << std::endl;
            std::cout << "Both files contain the same " << result.file1RowCount
                << " rows (including headers, ignoring order)." << std::endl;
            std::cout << "Decimal comparison: first 4 decimal places only." << std::endl;

            std::remove("only_in_file1.csv");
            std::remove("only_in_file2.csv");
        }
        else {
            std::cout << "FILES DIFFER" << std::endl;
            std::cout << std::endl;

            std::cout << "Summary:" << std::endl;
            std::cout << "  File 1 rows: " << result.file1RowCount << std::endl;
            std::cout << "  File 2 rows: " << result.file2RowCount << std::endl;
            std::cout << "  Rows only in File 1: " << result.onlyInFile1.size() << std::endl;
            std::cout << "  Rows only in File 2: " << result.onlyInFile2.size() << std::endl;
            std::cout << std::endl;

            if (!result.onlyInFile1.empty()) {
                std::cout << "Rows only in File 1 (" << file1 << "):" << std::endl;
                size_t displayCount = std::min(result.onlyInFile1.size(), size_t(10));
                for (size_t i = 0; i < displayCount; ++i) {
                    std::cout << "  " << formatRow(result.onlyInFile1[i]) << std::endl;
                }
                if (result.onlyInFile1.size() > 10) {
                    std::cout << "  ... and " << (result.onlyInFile1.size() - 10)
                        << " more rows" << std::endl;
                }
                std::cout << std::endl;
            }

            if (!result.onlyInFile2.empty()) {
                std::cout << "Rows only in File 2 (" << file2 << "):" << std::endl;
                size_t displayCount = std::min(result.onlyInFile2.size(), size_t(10));
                for (size_t i = 0; i < displayCount; ++i) {
                    std::cout << "  " << formatRow(result.onlyInFile2[i]) << std::endl;
                }
                if (result.onlyInFile2.size() > 10) {
                    std::cout << "  ... and " << (result.onlyInFile2.size() - 10)
                        << " more rows" << std::endl;
                }
                std::cout << std::endl;
            }

            comparator.writeRowsToCSV("only_in_file1.csv", result.onlyInFile1);
            comparator.writeRowsToCSV("only_in_file2.csv", result.onlyInFile2);

            std::cout << "Output files created:" << std::endl;
            std::cout << "  only_in_file1.csv (" << result.onlyInFile1.size() << " rows)" << std::endl;
            std::cout << "  only_in_file2.csv (" << result.onlyInFile2.size() << " rows)" << std::endl;
            std::cout << std::endl;

            return 1;
        }

    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}