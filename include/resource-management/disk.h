#pragma once
#include <unordered_set>
#include <filesystem>
#include "../internals.h"

class FileExt{
private:
    char ext[9];
public:
    FileExt(const fs::path &);
    const char* getExtension() const;
};

class TypeIterator{
private:
    #define FILEITER std::unordered_set<fs::path>::iterator
    FileExt type;
    FILEITER current;
    FILEITER nxt;
    FILEITER end;
public:
    TypeIterator(FileExt type, FILEITER head, FILEITER tail);
    FileExt getType() const;
    bool hasNext() const;
    File next();
    #undef FILEITER
};

class FileMgr{
private:
    uset<fs::path> registered;
    umap<FileExt,uset<fs::path>> files;
    umap<FileExt,uset<fs::path>> releaseList;
protected:
    void addFile(fs::path&);
    void removeFile(fs::path&);
public:
    TypeIterator find(FileExt);
    void load(const char*);
    void unload(const char*);
};