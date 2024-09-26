#pragma once
#ifndef ASSET_LOADER_H
#define ASSET_LOADER_H
#include <templates/singleton.h>
#include <resources/fileio/file-mgr.h>

namespace CE::Assets {
    struct Loader : Singleton_CTS<Loader>, FileMgr {
        explicit Loader(const std::filesystem::path& root_path) : FileMgr(root_path) {}
        void load_assets();
    };
}

#endif //ASSET_LOADER_H
