#pragma once
#ifndef RES_DISK_H
#define RES_DISK_H

#include <unordered_set>
#include <unordered_map>
#include <filesystem>

#include <internals.h>

namespace fs = std::filesystem;

class FileExt{
private:
    char ext[9] = {};
public:
    explicit FileExt(const fs::path &);

    [[nodiscard("getExtension called, return value is unused")]]
    const char* getExtension() const;
    bool operator==(const FileExt &rhs) const;
    bool operator!=(const FileExt &rhs) const;

};

class TypeIterator{
private:
    using FileIter = std::unordered_set<fs::path>::iterator;
    FileExt type;
    FileIter current;
    FileIter nxt;
    FileIter end;
public:
    TypeIterator(FileExt type, FileIter head, FileIter tail);

    [[nodiscard("getType called, return value is unused")]]
    FileExt getType() const;
    [[nodiscard("hasNext called, return value is unused")]]
    bool hasNext() const;
    fs::path next();
};

namespace std {
    template <>
    struct hash<FileExt> {
        size_t operator()(const FileExt& k) const {
            return hash<string>()(k.getExtension());
        }
    };
}

#endif
