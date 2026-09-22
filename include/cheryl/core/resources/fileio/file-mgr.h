#pragma once
#include <filesystem>
#include <unordered_set>
#include <unordered_map>
#include <vector>

using f_ext = std::string;
using fspath = std::filesystem::path;

class FileMgr {
public:
    FileMgr(const std::filesystem::path& root_path);
    void search_directory(const std::filesystem::path& directory);
    const std::vector<fspath>& get_files_of_type(f_ext extension);
private:
    std::unordered_set<std::filesystem::path> directories;
    std::unordered_map<f_ext, std::vector<fspath>> mapped_files;
};