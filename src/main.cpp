#include "csv_comparator.h"
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
        std::cerr << "Usage: " << argv[0] << " <file1.csv> <file2.csv>" << std::endl;
        std::cerr << std::endl;
        std::cerr << "CSV Comparator - High-performance CSV file comparison" << std::endl;
        std::cerr << "Compares two CSV files and reports differences." << std::endl;
        std::cerr << "Files can be in any order (order-independent comparison)." << std::endl;
        std::cerr << "Decimal numbers are compared to 4 decimal places." << std::endl;
        return 1;
    }

    try {
        std::string file1 = argv[1];
        std::string file2 = argv[2];

        CSVComparator comparator;
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
                for (const auto& row : result.onlyInFile1) {
                    std::cout << "  " << formatRow(row) << std::endl;
                }
                std::cout << std::endl;
            }

            if (!result.onlyInFile2.empty()) {
                std::cout << "Rows only in File 2 (" << file2 << "):" << std::endl;
                for (const auto& row : result.onlyInFile2) {
                    std::cout << "  " << formatRow(row) << std::endl;
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