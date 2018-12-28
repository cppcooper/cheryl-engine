#pragma once
#include <unordered_map>
#include "../internals.h"
#include "../interfaces/loader.h"
#include "../interfaces/loadable.h"

class ResourceMgr : public Singleton<ResourceMgr>{
    using FileExt = std::filesystem::path;
    using File = std::filesystem::path;
private:
    Singleton<FileMgr> &file_mgr = Singleton<FileMgr>::get();
    std::unordered_map<FileExt,Loader*> loaders;
    std::unordered_map<File,loadable*> loaded; //mapped according to file name, unloaded based on path.. mapping replaced if duplicate name exists
public:
};