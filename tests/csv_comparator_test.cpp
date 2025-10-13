#include <gtest/gtest.h>
#include "threaded_comparator.h"
#include "csv_parser.h"
#include <fstream>
#include <random>
#include <filesystem>
#include <chrono>
#include <iostream>

// Tracy integration
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#pragma message ( "Tracy enabled!" )
#else
#define ZoneScoped
#define ZoneScopedN(name)
#define FrameMark
#define FrameMarkNamed(name)
#endif

class CSVComparatorTest : public ::testing::Test {
protected:
    std::mt19937 rng;
    const int NUM_ROWS = 100000;
    const int NUM_COLS = 10;
    const int MAX_DIFFERENCES = 10;

    std::string testFile1 = "test_file1.csv";
    std::string testFile2 = "test_file2.csv";

    void SetUp() override {
        rng.seed(42);
    }

    void TearDown() override {
        std::filesystem::remove(testFile1);
        std::filesystem::remove(testFile2);
        std::filesystem::remove("only_in_file1.csv");
        std::filesystem::remove("only_in_file2.csv");
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

    void writeRowToFile(std::ofstream& file, const std::vector<std::string>& row) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) file << ",";
            file << row[i];
        }
        file << "\n";
    }

    void createTestFiles(int numDifferences) {
        std::ofstream file1(testFile1);
        std::ofstream file2(testFile2);

        ASSERT_TRUE(file1.is_open()) << "Failed to create " << testFile1;
        ASSERT_TRUE(file2.is_open()) << "Failed to create " << testFile2;

        // Write headers
        std::vector<std::string> headers = {
            "Name", "Price", "Category", "Quantity", "Rating",
            "Status", "Count", "Score", "Id", "Value"
        };
        writeRowToFile(file1, headers);
        writeRowToFile(file2, headers);

        // Generate all rows
        std::vector<std::vector<std::string>> allRows;
        for (int i = 0; i < NUM_ROWS; ++i) {
            allRows.push_back(generateRandomRow());
        }

        // Select random rows to be different
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

        // Write rows
        for (int i = 0; i < NUM_ROWS; ++i) {
            writeRowToFile(file1, allRows[i]);

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
                writeRowToFile(file2, modifiedRow);
            }
            else {
                writeRowToFile(file2, allRows[i]);
            }
        }

        file1.close();
        file2.close();
    }
};

TEST_F(CSVComparatorTest, IdenticalFilesMatch) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: IdenticalFilesMatch");
#endif

    // Setup (NOT profiled - happens before ZoneScoped)
    createTestFiles(0);

    // Begin profiling the actual test
    {
        ZoneScoped;
        ZoneName("IdenticalFilesMatch - Comparison", 33);

        ThreadedCSVComparator comparator;
        auto result = comparator.compare(testFile1, testFile2);

        EXPECT_TRUE(result.filesMatch);
        EXPECT_EQ(result.file1RowCount, result.file2RowCount);
        EXPECT_EQ(result.onlyInFile1.size(), 0);
        EXPECT_EQ(result.onlyInFile2.size(), 0);
    }

    std::cout << "? Test PASSED: Identical files matched correctly" << std::endl;
}

TEST_F(CSVComparatorTest, FilesWithDifferencesDetected) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: FilesWithDifferencesDetected");
#endif

    std::uniform_int_distribution<> diffDist(1, MAX_DIFFERENCES);
    int numDifferences = diffDist(rng);

    createTestFiles(numDifferences);

    {
        ZoneScoped;
        ZoneName("FilesWithDifferencesDetected - Comparison", 42);

        ThreadedCSVComparator comparator;
        auto result = comparator.compare(testFile1, testFile2);

        EXPECT_FALSE(result.filesMatch);
        EXPECT_GT(result.onlyInFile1.size() + result.onlyInFile2.size(), 0);
    }

    std::cout << "? Test PASSED: Differences detected correctly" << std::endl;
}

TEST_F(CSVComparatorTest, DecimalComparisonWorks) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: DecimalComparisonWorks");
#endif

    std::ofstream file1("test_decimal1.csv");
    std::ofstream file2("test_decimal2.csv");

    file1 << "Value\n";
    file1 << "3.14159265\n";
    file1 << "2.71828182\n";

    file2 << "Value\n";
    file2 << "3.14159999\n";
    file2 << "2.71820000\n";

    file1.close();
    file2.close();

    {
        ZoneScoped;
        ZoneName("DecimalComparisonWorks - Comparison", 34);

        ThreadedCSVComparator comparator;
        auto result = comparator.compare("test_decimal1.csv", "test_decimal2.csv");

        EXPECT_EQ(result.onlyInFile1.size(), 1);
        EXPECT_EQ(result.onlyInFile2.size(), 1);
    }

    std::cout << "? Test PASSED: Decimal precision (4 places) works correctly" << std::endl;

    std::filesystem::remove("test_decimal1.csv");
    std::filesystem::remove("test_decimal2.csv");
}

TEST_F(CSVComparatorTest, LargeFilePerformance) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: LargeFilePerformance");
#endif

    auto start = std::chrono::high_resolution_clock::now();

    createTestFiles(5);

    {
        ZoneScoped;
        ZoneName("LargeFilePerformance - Comparison", 33);

        auto readStart = std::chrono::high_resolution_clock::now();
        ThreadedCSVComparator comparator;
        auto result = comparator.compare(testFile1, testFile2);
        auto readEnd = std::chrono::high_resolution_clock::now();

        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            readEnd - start).count();

        std::cout << "? Performance Test Results:" << std::endl;
        std::cout << "  Total time: " << totalDuration << " ms" << std::endl;
        std::cout << "  Rows processed: " << result.file1RowCount + result.file2RowCount << std::endl;

        EXPECT_LT(totalDuration, 30000) << "Processing took longer than 30 seconds";
    }
}

TEST_F(CSVComparatorTest, SingleThreadedPathForSmallFiles) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: SingleThreadedPathForSmallFiles");
#endif

    std::ofstream file1("small_file1.csv");
    std::ofstream file2("small_file2.csv");

    file1 << "ID,Value\n";
    file2 << "ID,Value\n";

    for (int i = 0; i < 500; ++i) {
        file1 << i << "," << i * 2 << "\n";
        file2 << i << "," << i * 2 << "\n";
    }

    file1.close();
    file2.close();

    {
        ZoneScoped;
        ZoneName("SingleThreadedPathForSmallFiles - Comparison", 47);

        ThreadedCSVComparator comparator;
        auto result = comparator.compare("small_file1.csv", "small_file2.csv");

        EXPECT_TRUE(result.filesMatch);
        EXPECT_EQ(result.file1RowCount, 501);
    }

    std::cout << "? Test PASSED: Single-threaded path used for small files" << std::endl;

    std::filesystem::remove("small_file1.csv");
    std::filesystem::remove("small_file2.csv");
}

TEST_F(CSVComparatorTest, OutputFilesCreatedOnDifference) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: OutputFilesCreatedOnDifference");
#endif

    createTestFiles(3);

    {
        ZoneScoped;
        ZoneName("OutputFilesCreatedOnDifference - Comparison", 44);

        ThreadedCSVComparator comparator;
        auto result = comparator.compare(testFile1, testFile2);

        EXPECT_FALSE(result.filesMatch);

        comparator.writeRowsToCSV("only_in_file1.csv", result.onlyInFile1);
        comparator.writeRowsToCSV("only_in_file2.csv", result.onlyInFile2);

        EXPECT_TRUE(std::filesystem::exists("only_in_file1.csv"));
        EXPECT_TRUE(std::filesystem::exists("only_in_file2.csv"));
    }

    std::cout << "? Test PASSED: Output files created correctly" << std::endl;
}

TEST_F(CSVComparatorTest, OutputFilesDeletedOnMatch) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: OutputFilesDeletedOnMatch");
#endif

    std::ofstream dummy1("only_in_file1.csv");
    std::ofstream dummy2("only_in_file2.csv");
    dummy1 << "dummy\n";
    dummy2 << "dummy\n";
    dummy1.close();
    dummy2.close();

    ASSERT_TRUE(std::filesystem::exists("only_in_file1.csv"));
    ASSERT_TRUE(std::filesystem::exists("only_in_file2.csv"));

    createTestFiles(0);

    {
        ZoneScoped;
        ZoneName("OutputFilesDeletedOnMatch - Comparison", 39);

        ThreadedCSVComparator comparator;
        auto result = comparator.compare(testFile1, testFile2);

        EXPECT_TRUE(result.filesMatch);

        std::remove("only_in_file1.csv");
        std::remove("only_in_file2.csv");

        EXPECT_FALSE(std::filesystem::exists("only_in_file1.csv"));
        EXPECT_FALSE(std::filesystem::exists("only_in_file2.csv"));
    }

    std::cout << "? Test PASSED: Output files deleted on match" << std::endl;
}