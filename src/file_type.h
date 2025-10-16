#pragma once

#include <string>
#include <fstream>
#include <algorithm>

enum class FileType {
    CSV,
    XLSX,
    UNKNOWN
};

class FileTypeDetector {
public:
    static FileType detect(const std::string& filename);
    static std::string toString(FileType type);

private:
    static FileType detectByExtension(const std::string& filename);
    static FileType detectByMagicBytes(const std::string& filename);
    static bool endsWith(const std::string& str, const std::string& suffix);
};