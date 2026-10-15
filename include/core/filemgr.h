#pragma once
#ifndef CHERYL_ENGINE_FILEMGR_H
#define CHERYL_ENGINE_FILEMGR_H

#include <resource-management/disk.h>

namespace CherylE {

    class FileMgr {
    private:
        template<class T> using uset = std::unordered_set<T>;
        template<class K, class V> using umap = std::unordered_map<K, V>;
        uset<fs::path> registered;
        umap<FileExt, uset<fs::path>> files;
        umap<FileExt, uset<fs::path>> releaseList;
    protected:
        void addFile(fs::path &);
        void removeFile(fs::path &);

    public:
        TypeIterator find(FileExt);
        void load(const char*);
        void unload(const char*);
    };
}
#endif //CHERYL_ENGINE_FILEMGR_H
