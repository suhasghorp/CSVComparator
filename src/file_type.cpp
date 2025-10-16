#include "file_type.h"

FileType FileTypeDetector::detect(const std::string& filename) {
    // First try extension-based detection (fast)
    FileType type = detectByExtension(filename);
    if (type != FileType::UNKNOWN) {
        return type;
    }

    // Fallback to magic byte detection (slower but more reliable)
    return detectByMagicBytes(filename);
}

FileType FileTypeDetector::detectByExtension(const std::string& filename) {
    if (endsWith(filename, ".csv")) {
        return FileType::CSV;
    }
    if (endsWith(filename, ".xlsx")) {
        return FileType::XLSX;
    }
    return FileType::UNKNOWN;
}

FileType FileTypeDetector::detectByMagicBytes(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return FileType::UNKNOWN;
    }

    // Read first 4 bytes
    char magic[4] = { 0 };
    file.read(magic, 4);

    if (!file) {
        return FileType::UNKNOWN;
    }

    // XLSX is a ZIP file with magic bytes: 0x50 0x4B 0x03 0x04 (PK..)
    if (magic[0] == 0x50 && magic[1] == 0x4B &&
        magic[2] == 0x03 && magic[3] == 0x04) {
        return FileType::XLSX;
    }

    // CSV is plain text, no specific magic number
    // If it's not XLSX and looks like text, assume CSV
    return FileType::CSV;
}

bool FileTypeDetector::endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }

    return std::equal(
        suffix.rbegin(),
        suffix.rend(),
        str.rbegin(),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a)) ==
                std::tolower(static_cast<unsigned char>(b));
        }
    );
}

std::string FileTypeDetector::toString(FileType type) {
    switch (type) {
    case FileType::CSV:  return "CSV";
    case FileType::XLSX: return "XLSX";
    default:             return "UNKNOWN";
    }
}