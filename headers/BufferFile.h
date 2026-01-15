#pragma once
#include <string>
#include <vector>
#include <cstdio>
#include <cstdint>

class BufferFile {
public:
    explicit BufferFile(const std::string& path, std::size_t bufferSize = 4096);
    ~BufferFile();

    bool isOpen() const;
    bool hasEnded() const;

    void seek(std::int64_t pos);
    void skip(std::size_t count);
    void resizeBuffer(std::size_t newSize);

    uint8_t readByte();
    std::vector<uint8_t> readRange(std::int64_t size);
    int textSearch(const std::string& text);
    void copy(uint8_t* target, std::int64_t offset, std::int64_t size = 0);
    std::int64_t getFilePos();
    std::int64_t getBufPos();
    std::int64_t getBufRange();

private:
    void updateBuffer();

    FILE* file_ = nullptr;
    std::vector<uint8_t> buffer_;
    std::int64_t bufRange_ = 0;
    std::int64_t bufPos_ = 0;
    std::int64_t filePos_ = 0;
    std::int64_t curSeek_ = 0;
    bool fileEnded_ = false;
};