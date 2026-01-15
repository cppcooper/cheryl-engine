#pragma once
#include <templates/singleton.h>
#include <core/resources/fileio/file-mgr.h>

namespace CE::Assets {
    struct Loader : Singleton_CTS<Loader>, FileMgr {
        explicit Loader(const std::filesystem::path& root_path) : FileMgr(root_path) {}
        void load_assets();
    };
}
