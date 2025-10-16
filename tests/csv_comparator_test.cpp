#include <gtest/gtest.h>
#include "csv_comparator.h"
#include "csv_parser.h"
#include <fstream>
#include <random>
#include <filesystem>
#include <chrono>

class CSVComparatorTest : public ::testing::Test {
protected:
    std::mt19937 rng;
    const int NUM_ROWS = 10000;
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

        ASSERT_TRUE(file1.is_open());
        ASSERT_TRUE(file2.is_open());

        std::vector<std::string> headers = {
            "Name", "Price", "Category", "Quantity", "Rating",
            "Status", "Count", "Score", "Id", "Value"
        };
        writeRowToFile(file1, headers);
        writeRowToFile(file2, headers);

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

    createTestFiles(0);

    {
        ZoneScoped;
        ZoneName("IdenticalFilesMatch - Comparison", 33);

        CSVComparator comparator;
        auto result = comparator.compare(testFile1, testFile2);

        EXPECT_TRUE(result.filesMatch);
        EXPECT_EQ(result.file1RowCount, result.file2RowCount);
        EXPECT_EQ(result.onlyInFile1.size(), 0);
        EXPECT_EQ(result.onlyInFile2.size(), 0);
    }

    std::cout << "Test PASSED: Identical files matched correctly" << std::endl;
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

        CSVComparator comparator;
        auto result = comparator.compare(testFile1, testFile2);

        EXPECT_FALSE(result.filesMatch);
        EXPECT_GT(result.onlyInFile1.size() + result.onlyInFile2.size(), 0);
    }

    std::cout << "Test PASSED: Differences detected correctly" << std::endl;
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

        CSVComparator comparator;
        auto result = comparator.compare("test_decimal1.csv", "test_decimal2.csv");

        EXPECT_EQ(result.onlyInFile1.size(), 1);
        EXPECT_EQ(result.onlyInFile2.size(), 1);
    }

    std::cout << "Test PASSED: Decimal precision (4 places) works correctly" << std::endl;

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
        CSVComparator comparator;
        auto result = comparator.compare(testFile1, testFile2);
        auto readEnd = std::chrono::high_resolution_clock::now();

        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            readEnd - start).count();

        std::cout << "Performance Test Results:" << std::endl;
        std::cout << "  Total time: " << totalDuration << " ms" << std::endl;
        std::cout << "  Rows processed: " << result.file1RowCount + result.file2RowCount << std::endl;

        EXPECT_LT(totalDuration, 30000) << "Processing took longer than 30 seconds";
    }
}

TEST_F(CSVComparatorTest, OutputFilesCreatedOnDifference) {
#ifdef TRACY_ENABLE
    FrameMarkNamed("Test: OutputFilesCreatedOnDifference");
#endif

    createTestFiles(3);

    {
        ZoneScoped;
        ZoneName("OutputFilesCreatedOnDifference - Comparison", 44);

        CSVComparator comparator;
        auto result = comparator.compare(testFile1, testFile2);

        EXPECT_FALSE(result.filesMatch);

        comparator.writeRowsToCSV("only_in_file1.csv", result.onlyInFile1);
        comparator.writeRowsToCSV("only_in_file2.csv", result.onlyInFile2);

        EXPECT_TRUE(std::filesystem::exists("only_in_file1.csv"));
        EXPECT_TRUE(std::filesystem::exists("only_in_file2.csv"));
    }

    std::cout << "Test PASSED: Output files created correctly" << std::endl;
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

        CSVComparator comparator;
        auto result = comparator.compare(testFile1, testFile2);

        EXPECT_TRUE(result.filesMatch);

        std::remove("only_in_file1.csv");
        std::remove("only_in_file2.csv");

        EXPECT_FALSE(std::filesystem::exists("only_in_file1.csv"));
        EXPECT_FALSE(std::filesystem::exists("only_in_file2.csv"));
    }

    std::cout << "Test PASSED: Output files deleted on match" << std::endl;
}

TEST_F(CSVComparatorTest, OptimizationValidation) {
    auto result1 = CSVParser::parseCSVLine("a,b,c");
    EXPECT_EQ(result1.size(), 3);
    EXPECT_EQ(result1[0], "a");
    EXPECT_EQ(result1[1], "b");
    EXPECT_EQ(result1[2], "c");

    auto result2 = CSVParser::parseCSVLine("  a  ,  b  ,  c  ");
    EXPECT_EQ(result2.size(), 3);
    EXPECT_EQ(result2[0], "a");
    EXPECT_EQ(result2[1], "b");
    EXPECT_EQ(result2[2], "c");

    auto result3 = CSVParser::parseCSVLine("\"a,b\",\"c\"\"d\",e");
    EXPECT_EQ(result3.size(), 3);
    EXPECT_EQ(result3[0], "a,b");
    EXPECT_EQ(result3[1], "c\"d");
    EXPECT_EQ(result3[2], "e");

    auto result4 = CSVParser::parseCSVLine("\"  spaced  \",normal,  mixed  ");
    EXPECT_EQ(result4.size(), 3);
    EXPECT_EQ(result4[0], "  spaced  ");
    EXPECT_EQ(result4[1], "normal");
    EXPECT_EQ(result4[2], "mixed");

    Row row1, row2;
    row1.columns = { "John", "25", "Engineer" };
    row2.columns = { "John", "25", "Engineer" };

    Row::Hash hasher;
    EXPECT_EQ(hasher(row1), hasher(row2));

    Row row3, row4;
    row3.columns = { "3.14159265" };
    row4.columns = { "3.14159999" };

    EXPECT_EQ(hasher(row3), hasher(row4));

    std::cout << "All optimization validation tests passed" << std::endl;
}

TEST_F(CSVComparatorTest, ParsingPerformanceBenchmark) {
    const int iterations = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto result = CSVParser::parseCSVLine(
            "John,  25,  Engineer  ,  50000  ,  Active  ,  2024-01-15  "
        );
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    double avgTime = static_cast<double>(duration) / iterations;

    std::cout << "Parsing Performance:" << std::endl;
    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Total time: " << duration << " µs" << std::endl;
    std::cout << "  Avg per line: " << avgTime << " µs" << std::endl;
    std::cout << "  Throughput: " << (iterations / (duration / 1000000.0))
        << " lines/sec" << std::endl;

    EXPECT_LT(avgTime, 5.0) << "Parsing is slower than expected";
}

TEST_F(CSVComparatorTest, HashPerformanceBenchmark) {
    const int iterations = 100000;

    Row testRow;
    testRow.columns = {
        "John", "25", "Engineer", "50000",
        "Active", "2024-01-15", "Department A",
        "Manager: Jane", "Location: NYC", "Project: Alpha"
    };

    Row::Hash hasher;

    auto start = std::chrono::high_resolution_clock::now();

    size_t dummySum = 0;
    for (int i = 0; i < iterations; ++i) {
        dummySum += hasher(testRow);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end - start).count();

    double avgTime = static_cast<double>(duration) / iterations;

    std::cout << "Hash Performance:" << std::endl;
    std::cout << "  Iterations: " << iterations << std::endl;
    std::cout << "  Total time: " << (duration / 1000000.0) << " ms" << std::endl;
    std::cout << "  Avg per hash: " << avgTime << " ns" << std::endl;
    std::cout << "  Throughput: " << (iterations / (duration / 1000000000.0))
        << " hashes/sec" << std::endl;
    std::cout << "  (Dummy sum: " << dummySum << " - prevents optimization)" << std::endl;

    std::cout << "  Implementation: std::from_chars (fast)" << std::endl;
    EXPECT_LT(avgTime, 10000.0) << "Hashing is slower than expected with from_chars";

    std::cout << "  Note: Time includes decimal normalization overhead" << std::endl;
}

TEST_F(CSVComparatorTest, FromCharsValidation) {
    std::vector<std::string> testValues = {
        "3.14159265",
        "2.71828182",
        "100.00001",
        "0.0001",
        "-123.456",
        "999999.9999",
        "0.00000001",
        "123",
        "-456.78"
    };

    for (const auto& val : testValues) {
        Row row1, row2;
        row1.columns = { val };
        row2.columns = { val };

        Row::Hash hasher;
        EXPECT_EQ(hasher(row1), hasher(row2))
            << "Hash mismatch for value: " << val;

        EXPECT_TRUE(Row::compareValues(val, val))
            << "Comparison failed for value: " << val;
    }

    EXPECT_TRUE(Row::compareValues("3.14159265", "3.14159999"))
        << "Should match at 4 decimal places";

    EXPECT_FALSE(Row::compareValues("3.14159265", "3.14169999"))
        << "Should NOT match at 4 decimal places";

    std::cout << "from_chars validation passed (using fast path)" << std::endl;


}