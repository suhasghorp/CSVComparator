#pragma once

#include "row.h"
#include <string>
#include <unordered_set>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <boost/lockfree/queue.hpp>

// Tracy profiler integration
#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
// Define empty macros when Tracy is disabled
#define ZoneScoped
#define ZoneScopedN(name)
#define ZoneName(name, size)
#define TracyPlot(name, value)
#define FrameMark
#define FrameMarkStart(name)
#define FrameMarkEnd(name)
#endif

class ThreadedCSVComparator {
public:
    ThreadedCSVComparator();
    ~ThreadedCSVComparator();

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
    static constexpr size_t QUEUE_CAPACITY = 10000;
    static constexpr size_t ROW_THRESHOLD = 1000;

    size_t countRows(const std::string& filename);
    ComparisonResult compareSingleThreaded(const std::string& file1, const std::string& file2);
    ComparisonResult compareMultiThreaded(const std::string& file1, const std::string& file2);

    void readerThread(const std::string& filename,
        boost::lockfree::queue<std::string*>& queue,
        std::atomic<bool>& complete,
        std::atomic<bool>& errorFlag);

    void parserThread(boost::lockfree::queue<std::string*>& queue1,
        boost::lockfree::queue<std::string*>& queue2,
        std::atomic<bool>& file1Complete,
        std::atomic<bool>& file2Complete,
        std::unordered_set<Row, Row::Hash>& rows1,
        std::unordered_set<Row, Row::Hash>& rows2,
        std::mutex& rows1Mutex,
        std::mutex& rows2Mutex,
        std::atomic<bool>& errorFlag);

    std::unordered_set<Row, Row::Hash> readCSV(const std::string& filename);
};