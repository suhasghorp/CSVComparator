#include "threaded_comparator.h"
#include "csv_parser.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>

ThreadedCSVComparator::ThreadedCSVComparator() = default;
ThreadedCSVComparator::~ThreadedCSVComparator() = default;

size_t ThreadedCSVComparator::countRows(const std::string& filename) {
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

std::unordered_set<Row, Row::Hash> ThreadedCSVComparator::readCSV(const std::string& filename) {
    ZoneScoped;
    ZoneName("Read CSV (Single-threaded)", 26);

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

void ThreadedCSVComparator::writeRowsToCSV(const std::string& filename, const std::vector<Row>& rows) {
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

ThreadedCSVComparator::ComparisonResult
ThreadedCSVComparator::compareSingleThreaded(const std::string& file1, const std::string& file2) {
    ZoneScoped;
    ZoneName("Single-threaded Comparison", 27);

    std::cout << "Using single-threaded comparison..." << std::endl;

    ComparisonResult result;

    {
        ZoneScoped;
        ZoneName("Read File 1", 11);
        auto rows1 = readCSV(file1);
        result.file1RowCount = rows1.size();
#ifdef TRACY_ENABLE
        TracyPlot("File 1 Rows", static_cast<int64_t>(rows1.size()));
#endif
    }

    {
        ZoneScoped;
        ZoneName("Read File 2", 11);
        auto rows2 = readCSV(file2);
        result.file2RowCount = rows2.size();
#ifdef TRACY_ENABLE
        TracyPlot("File 2 Rows", static_cast<int64_t>(rows2.size()));
#endif

        {
            ZoneScoped;
            ZoneName("Find Differences", 16);

            auto rows1Temp = readCSV(file1); // Re-read for comparison

            for (const auto& row : rows1Temp) {
                if (rows2.find(row) == rows2.end()) {
                    result.onlyInFile1.push_back(row);
                }
            }

            for (const auto& row : rows2) {
                if (rows1Temp.find(row) == rows1Temp.end()) {
                    result.onlyInFile2.push_back(row);
                }
            }
        }
    }

    result.filesMatch = result.onlyInFile1.empty() && result.onlyInFile2.empty();
#ifdef TRACY_ENABLE
    TracyPlot("Files Match", result.filesMatch ? static_cast < int64_t>(1) : static_cast < int64_t>(0));
#endif

    return result;
}

void ThreadedCSVComparator::readerThread(const std::string& filename,
    boost::lockfree::queue<std::string*>& queue,
    std::atomic<bool>& complete,
    std::atomic<bool>& errorFlag) {
    ZoneScoped;
    ZoneName("Reader Thread", 13);

#ifdef TRACY_ENABLE
    tracy::SetThreadName("CSV Reader");
#endif

    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Reader thread: Could not open file: " << filename << std::endl;
            errorFlag = true;
            complete = true;
            return;
        }

        std::string line;
        size_t linesRead = 0;

        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::string* linePtr = new std::string(std::move(line));

            while (!queue.push(linePtr)) {
                {
                    ZoneScoped;
                    ZoneName("Reader Waiting (Queue Full)", 28);
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }

                if (errorFlag) {
                    delete linePtr;
                    complete = true;
                    return;
                }
            }

            ++linesRead;
#ifdef TRACY_ENABLE
            if (linesRead % 1000 == 0) {
                TracyPlot("Lines Read", static_cast<int64_t>(linesRead));
            }
#endif
        }

        complete = true;
#ifdef TRACY_ENABLE
        TracyPlot("Total Lines Read", static_cast<int64_t>(linesRead));
#endif

    }
    catch (const std::exception& e) {
        std::cerr << "Reader thread exception: " << e.what() << std::endl;
        errorFlag = true;
        complete = true;
    }
}

void ThreadedCSVComparator::parserThread(boost::lockfree::queue<std::string*>& queue1,
    boost::lockfree::queue<std::string*>& queue2,
    std::atomic<bool>& file1Complete,
    std::atomic<bool>& file2Complete,
    std::unordered_set<Row, Row::Hash>& rows1,
    std::unordered_set<Row, Row::Hash>& rows2,
    std::mutex& rows1Mutex,
    std::mutex& rows2Mutex,
    std::atomic<bool>& errorFlag) {
    ZoneScoped;
    ZoneName("Parser Thread", 13);

#ifdef TRACY_ENABLE
    tracy::SetThreadName("CSV Parser");
#endif

    try {
        std::string* linePtr = nullptr;
        size_t rowsParsed = 0;

        while (true) {
            bool workDone = false;

            if (queue1.pop(linePtr)) {
                {
                    ZoneScoped;
                    ZoneName("Parse & Insert File1", 20);
                    Row row = CSVParser::parseCSVRow(*linePtr);
                    delete linePtr;
                    linePtr = nullptr;

                    {
                        ZoneScoped;
                        ZoneName("Lock File1 Mutex", 16);
                        std::lock_guard<std::mutex> lock(rows1Mutex);
                        rows1.insert(std::move(row));
                    }
                }
                workDone = true;
                ++rowsParsed;
            }

            if (queue2.pop(linePtr)) {
                {
                    ZoneScoped;
                    ZoneName("Parse & Insert File2", 20);
                    Row row = CSVParser::parseCSVRow(*linePtr);
                    delete linePtr;
                    linePtr = nullptr;

                    {
                        ZoneScoped;
                        ZoneName("Lock File2 Mutex", 16);
                        std::lock_guard<std::mutex> lock(rows2Mutex);
                        rows2.insert(std::move(row));
                    }
                }
                workDone = true;
                ++rowsParsed;
            }

            if (!workDone) {
                if (file1Complete && file2Complete &&
                    queue1.empty() && queue2.empty()) {
                    break;
                }

                {
                    ZoneScoped;
                    ZoneName("Parser Waiting", 14);
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }

            if (errorFlag) {
                break;
            }
        }

#ifdef TRACY_ENABLE
        TracyPlot("Rows Parsed", static_cast<int64_t>(rowsParsed));
#endif

    }
    catch (const std::exception& e) {
        std::cerr << "Parser thread exception: " << e.what() << std::endl;
        errorFlag = true;
    }
}

ThreadedCSVComparator::ComparisonResult
ThreadedCSVComparator::compareMultiThreaded(const std::string& file1, const std::string& file2) {
    ZoneScoped;
    ZoneName("Multi-threaded Comparison", 26);

    std::cout << "Using multi-threaded comparison..." << std::endl;

    boost::lockfree::queue<std::string*> queue1(QUEUE_CAPACITY);
    boost::lockfree::queue<std::string*> queue2(QUEUE_CAPACITY);

    std::unordered_set<Row, Row::Hash> rows1;
    std::unordered_set<Row, Row::Hash> rows2;
    std::mutex rows1Mutex;
    std::mutex rows2Mutex;

    std::atomic<bool> file1Complete{ false };
    std::atomic<bool> file2Complete{ false };
    std::atomic<bool> errorFlag{ false };

    unsigned int numParsers = std::max(2u, std::thread::hardware_concurrency() - 2);
    std::cout << "Using " << numParsers << " parser threads" << std::endl;
#ifdef TRACY_ENABLE
    TracyPlot("Parser Thread Count", static_cast<int64_t>(numParsers));
#endif

    {
        ZoneScoped;
        ZoneName("Threaded Processing", 19);

        std::thread reader1([this, &file1, &queue1, &file1Complete, &errorFlag]() {
            readerThread(file1, queue1, file1Complete, errorFlag);
            });

        std::thread reader2([this, &file2, &queue2, &file2Complete, &errorFlag]() {
            readerThread(file2, queue2, file2Complete, errorFlag);
            });

        std::vector<std::thread> parsers;
        for (unsigned int i = 0; i < numParsers; ++i) {
            parsers.emplace_back([this, &queue1, &queue2, &file1Complete, &file2Complete,
                &rows1, &rows2, &rows1Mutex, &rows2Mutex, &errorFlag]() {
                    parserThread(queue1, queue2, file1Complete, file2Complete,
                        rows1, rows2, rows1Mutex, rows2Mutex, errorFlag);
                });
        }

        reader1.join();
        reader2.join();

        for (auto& parser : parsers) {
            parser.join();
        }
    }

    if (errorFlag) {
        throw std::runtime_error("Error occurred during multi-threaded processing");
    }

    ComparisonResult result;
    result.file1RowCount = rows1.size();
    result.file2RowCount = rows2.size();

#ifdef TRACY_ENABLE
    TracyPlot("File 1 Rows", static_cast<int64_t>(rows1.size()));
    TracyPlot("File 2 Rows", static_cast<int64_t>(rows2.size()));
#endif

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
    TracyPlot("Files Match", result.filesMatch ? static_cast < int64_t>(1) : static_cast < int64_t>(0));
    TracyPlot("Differences Found", static_cast<int64_t>(result.onlyInFile1.size() + result.onlyInFile2.size()));
#endif

    return result;
}

ThreadedCSVComparator::ComparisonResult
ThreadedCSVComparator::compare(const std::string& file1, const std::string& file2) {
    ZoneScoped;
    ZoneName("CSV Compare", 11);

    std::cout << "Comparing files:" << std::endl;
    std::cout << "  File 1: " << file1 << std::endl;
    std::cout << "  File 2: " << file2 << std::endl;
    std::cout << std::endl;

    std::cout << "Counting rows..." << std::endl;
    size_t rows1 = countRows(file1);
    size_t rows2 = countRows(file2);
    std::cout << "  File 1: " << rows1 << " rows" << std::endl;
    std::cout << "  File 2: " << rows2 << " rows" << std::endl;
    std::cout << std::endl;

    ComparisonResult result;
    if (rows1 < ROW_THRESHOLD && rows2 < ROW_THRESHOLD) {
#ifdef TRACY_ENABLE
        TracyPlot("Comparison Mode", static_cast < int64_t>(0));  // 0 = single-threaded
#endif
        result = compareSingleThreaded(file1, file2);
    }
    else {
#ifdef TRACY_ENABLE
        TracyPlot("Comparison Mode", static_cast < int64_t>(1));  // 1 = multi-threaded
#endif
        result = compareMultiThreaded(file1, file2);
    }

    return result;
}