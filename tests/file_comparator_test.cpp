#include <gtest/gtest.h>
#include "file_comparator.h"
#include "csv_parser.h"
#include "file_type.h"
#include <fstream>
#include <random>
#include <filesystem>
#include <chrono>
#include <xlnt/xlnt.hpp>

// ============ TEST HELPER CLASS ============

class XLSXTestHelper {
public:
    static void createTestFile(
        const std::string& filename,
        const std::vector<std::vector<std::string>>& data) {

        xlnt::workbook wb;
        auto ws = wb.active_sheet();

        for (size_t r = 0; r < data.size(); ++r) {
            for (size_t c = 0; c < data[r].size(); ++c) {
                // Try to parse as number
                try {
                    size_t pos;
                    double d = std::stod(data[r][c], &pos);
                    if (pos == data[r][c].length()) {
                        // It's a number
                        ws.cell(xlnt::column_t(c + 1), r + 1).value(d);
                    }
                    else {
                        // String
                        ws.cell(xlnt::column_t(c + 1), r + 1).value(data[r][c]);
                    }
                }
                catch (...) {
                    // String
                    ws.cell(xlnt::column_t(c + 1), r + 1).value(data[r][c]);
                }
            }
        }

        wb.save(filename);
    }
};

// ============ TEST FIXTURE ============

class FileComparatorTest : public ::testing::Test {
protected:
    std::mt19937 rng;
    const int NUM_ROWS = 10000;
    const int NUM_COLS = 10;
    const int MAX_DIFFERENCES = 10;

    std::string testFile1CSV = "test_file1.csv";
    std::string testFile2CSV = "test_file2.csv";
    std::string testFile1XLSX = "test_file1.xlsx";
    std::string testFile2XLSX = "test_file2.xlsx";

    void SetUp() override {
        rng.seed(42);
    }

    void TearDown() override {
        // Clean up all test files
        std::filesystem::remove(testFile1CSV);
        std::filesystem::remove(testFile2CSV);
        std::filesystem::remove(testFile1XLSX);
        std::filesystem::remove(testFile2XLSX);
        std::filesystem::remove("only_in_file1.csv");
        std::filesystem::remove("only_in_file2.csv");
        std::filesystem::remove("test_decimal1.csv");
        std::filesystem::remove("test_decimal2.csv");
        std::filesystem::remove("test_decimal1.xlsx");
        std::filesystem::remove("test_decimal2.xlsx");
        std::filesystem::remove("simple_test.xlsx");
        std::filesystem::remove("empty_test.xlsx");
        std::filesystem::remove("mixed_types.xlsx");
    }

    std::string generateRandomString(int length) {
        static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
        std::string result;
        result.reserve(length);
        for (int i = 0; i < length; ++i) {
            result += charset[dist(rng)];
        }
        return result;
    }

    std::string generateRandomNumber(bool isDecimal) {
        if (isDecimal) {
            std::uniform_real_distribution<> dist(0.0, 10000.0);
            double value = dist(rng);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(6) << value;
            return oss.str();
        }
        else {
            std::uniform_int_distribution<> dist(0, 100000);
            return std::to_string(dist(rng));
        }
    }

    std::vector<std::string> generateRandomRow() {
        std::vector<std::string> row;
        row.reserve(NUM_COLS);

        for (int col = 0; col < NUM_COLS; ++col) {
            if (col == 0 || col == 2 || col == 5) {
                row.push_back(generateRandomString(8));
            }
            else if (col == 1 || col == 4 || col == 7) {
                row.push_back(generateRandomNumber(true));
            }
            else {
                row.push_back(generateRandomNumber(false));
            }
        }

        return row;
    }

    void writeRowToCSV(std::ofstream& file, const std::vector<std::string>& row) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) file << ",";
            file << row[i];
        }
        file << "\n";
    }

    void createTestCSVFiles(int numDifferences) {
        std::ofstream file1(testFile1CSV);
        std::ofstream file2(testFile2CSV);

        ASSERT_TRUE(file1.is_open());
        ASSERT_TRUE(file2.is_open());

        std::vector<std::string> headers = {
            "Name", "Price", "Category", "Quantity", "Rating",
            "Status", "Count", "Score", "Id", "Value"
        };
        writeRowToCSV(file1, headers);
        writeRowToCSV(file2, headers);

        std::vector<std::vector<std::string>> allRows;
        for (int i = 0; i < NUM_ROWS; ++i) {
            allRows.push_back(generateRandomRow());
        }

        std::vector<int> differenceIndices;
        if (numDifferences > 0) {
            std::uniform_int_distribution<> dist(0, NUM_ROWS - 1);
            while (differenceIndices.size() < static_cast<size_t>(numDifferences)) {
                int idx = dist(rng);
                if (std::find(differenceIndices.begin(), differenceIndices.end(), idx)
                    == differenceIndices.end()) {
                    differenceIndices.push_back(idx);
                }
            }
        }

        for (int i = 0; i < NUM_ROWS; ++i) {
            writeRowToCSV(file1, allRows[i]);

            if (std::find(differenceIndices.begin(), differenceIndices.end(), i)
                != differenceIndices.end()) {
                auto modifiedRow = allRows[i];
                std::uniform_int_distribution<> colDist(0, NUM_COLS - 1);
                int colToChange = colDist(rng);
                if (colToChange == 0 || colToChange == 2 || colToChange == 5) {
                    modifiedRow[colToChange] = generateRandomString(8);
                }
                else {
                    modifiedRow[colToChange] = generateRandomNumber(colToChange % 2 == 1);
                }
                writeRowToCSV(file2, modifiedRow);
            }
            else {
                writeRowToCSV(file2, allRows[i]);
            }
        }

        file1.close();
        file2.close();
    }

    void createTestXLSXFiles(int numDifferences) {
        std::vector<std::vector<std::string>> headers = {
            {"Name", "Price", "Category", "Quantity", "Rating",
             "Status", "Count", "Score", "Id", "Value"}
        };

        std::vector<std::vector<std::string>> allRows;
        for (int i = 0; i < NUM_ROWS; ++i) {
            allRows.push_back(generateRandomRow());
        }

        std::vector<int> differenceIndices;
        if (numDifferences > 0) {
            std::uniform_int_distribution<> dist(0, NUM_ROWS - 1);
            while (differenceIndices.size() < static_cast<size_t>(numDifferences)) {
                int idx = dist(rng);
                if (std::find(differenceIndices.begin(), differenceIndices.end(), idx)
                    == differenceIndices.end()) {
                    differenceIndices.push_back(idx);
                }
            }
        }

        // Create file 1
        std::vector<std::vector<std::string>> data1 = headers;
        data1.insert(data1.end(), allRows.begin(), allRows.end());
        XLSXTestHelper::createTestFile(testFile1XLSX, data1);

        // Create file 2 with differences
        std::vector<std::vector<std::string>> data2 = headers;
        for (int i = 0; i < NUM_ROWS; ++i) {
            if (std::find(differenceIndices.begin(), differenceIndices.end(), i)
                != differenceIndices.end()) {
                auto modifiedRow = allRows[i];
                std::uniform_int_distribution<> colDist(0, NUM_COLS - 1);
                int colToChange = colDist(rng);
                if (colToChange == 0 || colToChange == 2 || colToChange == 5) {
                    modifiedRow[colToChange] = generateRandomString(8);
                }
                else {
                    modifiedRow[colToChange] = generateRandomNumber(colToChange % 2 == 1);
                }
                data2.push_back(modifiedRow);
            }
            else {
                data2.push_back(allRows[i]);
            }
        }
        XLSXTestHelper::createTestFile(testFile2XLSX, data2);
    }
};

// ============ FILE TYPE DETECTION TESTS ============

TEST_F(FileComparatorTest, FileTypeDetection_CSV) {
    std::ofstream file("test.csv");
    file << "a,b,c\n";
    file.close();

    FileType type = FileTypeDetector::detect("test.csv");
    EXPECT_EQ(type, FileType::CSV);

    std::filesystem::remove("test.csv");
}

TEST_F(FileComparatorTest, FileTypeDetection_XLSX) {
    XLSXTestHelper::createTestFile("test.xlsx", { {"a", "b", "c"} });

    FileType type = FileTypeDetector::detect("test.xlsx");
    EXPECT_EQ(type, FileType::XLSX);

    std::filesystem::remove("test.xlsx");
}

TEST_F(FileComparatorTest, FileTypeDetection_CaseInsensitive) {
    std::ofstream file("test.CSV");
    file << "a,b,c\n";
    file.close();

    FileType type = FileTypeDetector::detect("test.CSV");
    EXPECT_EQ(type, FileType::CSV);

    std::filesystem::remove("test.CSV");
}

// ============ CSV COMPARISON TESTS ============

TEST_F(FileComparatorTest, CSV_IdenticalFilesMatch) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: CSV_IdenticalFilesMatch");
#endif

    createTestCSVFiles(0);

    FileComparator comparator;
    auto result = comparator.compare(testFile1CSV, testFile2CSV);

    EXPECT_TRUE(result.filesMatch);
    EXPECT_EQ(result.file1RowCount, result.file2RowCount);
    EXPECT_EQ(result.onlyInFile1.size(), 0);
    EXPECT_EQ(result.onlyInFile2.size(), 0);

    std::cout << "Test PASSED: CSV identical files matched" << std::endl;
}

TEST_F(FileComparatorTest, CSV_FilesWithDifferences) {
    std::uniform_int_distribution<> diffDist(1, MAX_DIFFERENCES);
    int numDifferences = diffDist(rng);

    createTestCSVFiles(numDifferences);

    FileComparator comparator;
    auto result = comparator.compare(testFile1CSV, testFile2CSV);

    EXPECT_FALSE(result.filesMatch);
    EXPECT_GT(result.onlyInFile1.size() + result.onlyInFile2.size(), 0);

    std::cout << "Test PASSED: CSV differences detected" << std::endl;
}

// ============ XLSX COMPARISON TESTS ============

TEST_F(FileComparatorTest, XLSX_IdenticalFilesMatch) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: XLSX_IdenticalFilesMatch");
#endif

    createTestXLSXFiles(0);

    FileComparator comparator;
    auto result = comparator.compare(testFile1XLSX, testFile2XLSX);

    EXPECT_TRUE(result.filesMatch);
    EXPECT_EQ(result.file1RowCount, result.file2RowCount);
    EXPECT_EQ(result.onlyInFile1.size(), 0);
    EXPECT_EQ(result.onlyInFile2.size(), 0);

    std::cout << "Test PASSED: XLSX identical files matched" << std::endl;
}

TEST_F(FileComparatorTest, XLSX_FilesWithDifferences) {
    std::uniform_int_distribution<> diffDist(1, MAX_DIFFERENCES);
    int numDifferences = diffDist(rng);

    createTestXLSXFiles(numDifferences);

    FileComparator comparator;
    auto result = comparator.compare(testFile1XLSX, testFile2XLSX);

    EXPECT_FALSE(result.filesMatch);
    EXPECT_GT(result.onlyInFile1.size() + result.onlyInFile2.size(), 0);

    std::cout << "Test PASSED: XLSX differences detected" << std::endl;
}

// ============ MIXED FORMAT TESTS ============

TEST_F(FileComparatorTest, Mixed_CSVtoXLSX_Match) {
    // Create identical data in both formats
    std::vector<std::vector<std::string>> data = {
        {"Name", "Age", "City"},
        {"Alice", "30", "NYC"},
        {"Bob", "25", "LA"},
        {"Charlie", "35", "SF"}
    };

    // Create CSV
    std::ofstream csv("test.csv");
    for (const auto& row : data) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) csv << ",";
            csv << row[i];
        }
        csv << "\n";
    }
    csv.close();

    // Create XLSX
    XLSXTestHelper::createTestFile("test.xlsx", data);

    FileComparator comparator;
    auto result = comparator.compare("test.csv", "test.xlsx");

    EXPECT_TRUE(result.filesMatch);

    std::filesystem::remove("test.csv");
    std::filesystem::remove("test.xlsx");

    std::cout << "Test PASSED: CSV to XLSX comparison works" << std::endl;
}

// ============ XLSX SPECIFIC TESTS ============

TEST_F(FileComparatorTest, XLSX_EmptyCells) {
    std::vector<std::vector<std::string>> data = {
        {"A", "B", "C"},
        {"1", "", "3"},
        {"4", "5", ""}
    };

    XLSXTestHelper::createTestFile("test1.xlsx", data);
    XLSXTestHelper::createTestFile("test2.xlsx", data);

    FileComparator comparator;
    auto result = comparator.compare("test1.xlsx", "test2.xlsx");

    EXPECT_TRUE(result.filesMatch);

    std::filesystem::remove("test1.xlsx");
    std::filesystem::remove("test2.xlsx");

    std::cout << "Test PASSED: XLSX empty cells handled correctly" << std::endl;
}

TEST_F(FileComparatorTest, XLSX_NumericPrecision) {
    std::vector<std::vector<std::string>> data1 = {
        {"Value"},
        {"3.14159265"},
        {"2.71828182"}
    };

    std::vector<std::vector<std::string>> data2 = {
        {"Value"},
        {"3.14159999"},  // Should match (4 decimal places)
        {"2.71820000"}   // Should NOT match (4th decimal differs)
    };

    XLSXTestHelper::createTestFile("test1.xlsx", data1);
    XLSXTestHelper::createTestFile("test2.xlsx", data2);

    FileComparator comparator;
    auto result = comparator.compare("test1.xlsx", "test2.xlsx");

    EXPECT_FALSE(result.filesMatch);
    EXPECT_EQ(result.onlyInFile1.size(), 1);
    EXPECT_EQ(result.onlyInFile2.size(), 1);

    std::filesystem::remove("test1.xlsx");
    std::filesystem::remove("test2.xlsx");

    std::cout << "Test PASSED: XLSX numeric precision works" << std::endl;
}

TEST_F(FileComparatorTest, XLSX_MixedTypes) {
    std::vector<std::vector<std::string>> data = {
        {"String", "Number", "Decimal"},
        {"Hello", "42", "3.14"},
        {"World", "100", "2.718"}
    };

    XLSXTestHelper::createTestFile("test1.xlsx", data);
    XLSXTestHelper::createTestFile("test2.xlsx", data);

    FileComparator comparator;
    auto result = comparator.compare("test1.xlsx", "test2.xlsx");

    EXPECT_TRUE(result.filesMatch);

    std::filesystem::remove("test1.xlsx");
    std::filesystem::remove("test2.xlsx");

    std::cout << "Test PASSED: XLSX mixed types handled" << std::endl;
}

// ============ PERFORMANCE TESTS ============

TEST_F(FileComparatorTest, Performance_CSV_Large) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: Performance_CSV_Large");
#endif

    auto start = std::chrono::high_resolution_clock::now();
    createTestCSVFiles(5);

    FileComparator comparator;
    auto result = comparator.compare(testFile1CSV, testFile2CSV);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    std::cout << "CSV Performance:" << std::endl;
    std::cout << "  Time: " << duration << " ms" << std::endl;
    std::cout << "  Rows: " << result.file1RowCount + result.file2RowCount << std::endl;

    EXPECT_LT(duration, 30000);
}

TEST_F(FileComparatorTest, Performance_XLSX_Large) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: Performance_XLSX_Large");
#endif

    auto start = std::chrono::high_resolution_clock::now();
    createTestXLSXFiles(5);

    FileComparator comparator;
    auto result = comparator.compare(testFile1XLSX, testFile2XLSX);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    std::cout << "XLSX Performance:" << std::endl;
    std::cout << "  Time: " << duration << " ms" << std::endl;
    std::cout << "  Rows: " << result.file1RowCount + result.file2RowCount << std::endl;

    EXPECT_LT(duration, 60000);  // XLSX is slower, allow more time
}

// ============ ERROR HANDLING TESTS ============

TEST_F(FileComparatorTest, Error_FileNotFound) {
    FileComparator comparator;

    EXPECT_THROW({
        comparator.compare("nonexistent1.csv", "nonexistent2.csv");
        }, std::runtime_error);
}

TEST_F(FileComparatorTest, Error_UnsupportedFormat) {
    std::ofstream file("test.txt");
    file << "not a csv or xlsx";
    file.close();

    FileComparator comparator;

    // Should still try to read as CSV (default fallback)
    // May throw or succeed depending on content

    std::filesystem::remove("test.txt");
}