#define _CRT_SECURE_NO_WARNINGS

#include <stdexcept>
#include <cstring>

#include "BufferFile.h"

#if defined(_WIN32)
#define fseeko _fseeki64
#define ftello _ftelli64
#elif !defined(_LARGEFILE_SOURCE)
#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#endif

BufferFile::BufferFile(const std::string& path, std::size_t bufferSize)
    : buffer_(bufferSize, 0)
{
    file_ = std::fopen(path.c_str(), "rb");
    if (!file_) {
        throw std::runtime_error("Could not open file: " + path);
    }

    filePos_ = 0;
    bufRange_ = std::fread(buffer_.data(), 1, buffer_.size(), file_);
    curSeek_ = filePos_ + bufRange_;
    fileEnded_ = (bufRange_ != buffer_.size());
}

BufferFile::~BufferFile() {
    if (file_) std::fclose(file_);
}

bool BufferFile::isOpen() const {
    return file_ != nullptr;
}

bool BufferFile::hasEnded() const {
    return fileEnded_;
}

void BufferFile::updateBuffer() {
    if (!fileEnded_ && file_) {
        filePos_ += bufPos_;
        fseeko(file_, filePos_ - curSeek_, SEEK_CUR);
        bufRange_ = std::fread(buffer_.data(), 1, buffer_.size(), file_);
        curSeek_ = filePos_ + bufRange_;
        fileEnded_ = (bufRange_ != buffer_.size());
        bufPos_ = 0;
    }
}

void BufferFile::seek(std::int64_t pos) {
    bool beyondRange = (pos - filePos_) >= static_cast<std::int64_t>(bufRange_);
    bufPos_ = pos - filePos_;
    if (beyondRange) {
        updateBuffer();
    }
}

void BufferFile::skip(std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
        if (bufPos_ >= bufRange_) {
            updateBuffer();
        }
        ++bufPos_;
    }
}

void BufferFile::resizeBuffer(std::size_t newSize) {
    buffer_.resize(newSize);
    updateBuffer();
}

uint8_t BufferFile::readByte() {
    if (bufPos_ >= bufRange_) {
        updateBuffer();
    }
    return buffer_[bufPos_++];
}

std::vector<uint8_t> BufferFile::readRange(std::size_t size) {
    if (bufPos_ + size >= bufRange_) {
        updateBuffer();
    }
    std::vector<uint8_t> range(size);
    std::memcpy(range.data(), buffer_.data() + bufPos_, size);
    bufPos_ += size;
    return range;
}

int BufferFile::textSearch(const std::string& text) {
    auto range = readRange(text.size()); // returns std::vector<uint8_t>
    std::string readStr(range.begin(), range.end());
    int res = std::strcmp(readStr.c_str(), text.c_str());
    return res == 0 ? 1 : 0;
}

void BufferFile::copy(uint8_t* target, std::size_t offset, std::size_t size) {
    if (bufPos_ + size >= bufRange_) {
        updateBuffer();
    }
    if (size == 0) {
        size = buffer_.size();
    }
    std::memcpy(target + offset, buffer_.data() + bufPos_, size);
    bufPos_ += size;
}

std::int64_t BufferFile::getFilePos()
{
    return filePos_;
}

std::int64_t BufferFile::getBufPos()
{
    return bufPos_;
}

std::int64_t BufferFile::getBufRange()
{
    return bufRange_;
}