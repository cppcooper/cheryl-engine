#pragma once
#include <unordered_map>

#include <internals.h>
#include <core/filemgr.h>
#include <abstracts/singleton.h>
#include <interfaces/loader.h>
#include <interfaces/loadable.h>
#include <resource-management/disk.h>

namespace CherylE{
    class ResourceMgr : public SuperSingleton<ResourceMgr> {
    TYPENAMEAVAILABLE_VIRTUAL
    private:
        FileMgr& file_mgr = *ConcreteSingleton<FileMgr>::construct();
        // todo: Policy definition? What is a Policy?
        //  std::unordered_map<FileExt, Policy*> policies;
        std::unordered_map<FileExt, Loader*> loaders;
        std::unordered_map<fs::path, loadable*> loaded; //mapped according to file name, unloaded based on path.. mapping replaced if duplicate name exists

    public:
        // todo: Policy?
        //  bool addType(const FileExt &, Policy*, Loader*);
        // todo: implement/revise
        void load(const char*);
        void unload(const char*);
        loadable* retrieve(fs::path &);
    };
}
