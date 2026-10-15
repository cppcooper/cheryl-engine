#include <fstream>

#include <core/filemgr.h>

namespace CherylE {
    void FileMgr::addFile(fs::path &file) {
        auto ext = FileExt(file.extension());
        auto iter = files.find(ext);
        if (iter != files.end()) {
            //add file to set
            if (!iter->second.emplace(file).second) {
                throw failed_operation(__CEFUNCTION__, __LINE__, "Duplicate file name.");
            }
        } else {
            //make a set, add file to set, add set to map
            std::unordered_set<fs::path> typeOfFile;
            typeOfFile.emplace(file);
            files.emplace(ext, typeOfFile);
        }
    }

    void FileMgr::removeFile(fs::path &file) {
        auto ext = FileExt(file.extension());
        auto iter = files.find(ext);
        if (iter != files.end()) {
            //remove file from set
            iter->second.erase(file);
            auto release_iter = releaseList.find(ext);
            if (release_iter != releaseList.end()) {
                if (!release_iter->second.emplace(file).second) {
                    throw failed_operation(__CEFUNCTION__, __LINE__, "Duplicate file name.");
                }
            } else {
                std::unordered_set<fs::path> typeOfFile;
                typeOfFile.emplace(file);
                releaseList.emplace(ext, typeOfFile);
            }
        } else {
            throw bad_request(__CEFUNCTION__, __LINE__, "File not found in File Manager.");
        }
    }

    TypeIterator FileMgr::find(FileExt type) {
        auto f_iter = files.find(type);
        TypeIterator iter(type, f_iter->second.begin(), f_iter->second.end());
        return iter;
    }

    void FileMgr::load(const char* manifest) {
        fs::path manifest_file(manifest);
        if (!fs::is_directory(manifest_file)) {
            std::fstream file(manifest_file);
            if (file.good()) {
                std::string line; //ie. directory
                //read directories from file
                while (std::getline(file, line)) {
                    if (file.fail()) {
                        throw failed_operation(__CEFUNCTION__, __LINE__, "Failed to read manifest.");
                    }
                    fs::path registrant(line);
                    if (fs::is_directory(registrant)) {
                        //iterator directory recursively
                        for (auto &entry: fs::recursive_directory_iterator(registrant)) {
                            auto p = entry.path();
                            //check if entry is a directory before adding the entry to the files list
                            if (!fs::is_directory(p)) {
                                addFile(p);
                            }
                        }
                    } else {
                        throw bad_request(__CEFUNCTION__, __LINE__, "Registrant is not a valid directory.");
                    }
                }
                if (file.fail()) {
                    throw failed_operation(__CEFUNCTION__, __LINE__, "Failed to read manifest.");
                }
            } else {
                throw failed_operation(__CEFUNCTION__, __LINE__, "Failed to open manifest.");
            }
        } else {
            throw invalid_args(__CEFUNCTION__, __LINE__, "Manifest not found. Invalid argument.");
        }
    }

    void FileMgr::unload(const char* manifest) {
        fs::path manifest_file(manifest);
        if (!fs::is_directory(manifest_file)) {
            std::fstream file(manifest_file);
            if (file.good()) {
                std::string line; //ie. directory
                //read directories from file
                while (std::getline(file, line)) {
                    if (file.fail()) {
                        throw failed_operation(__CEFUNCTION__, __LINE__, "Failed to read manifest.");
                    }
                    fs::path registrant(line);
                    if (fs::is_directory(registrant)) {
                        //iterator directory recursively
                        for (auto &entry: fs::recursive_directory_iterator(registrant)) {
                            auto p = entry.path();
                            //check if entry is a directory before adding the entry to the files list
                            if (!fs::is_directory(p)) {
                                removeFile(p);
                            }
                        }
                    } else {
                        throw bad_request(__CEFUNCTION__, __LINE__, "Registrant is not a valid directory.");
                    }
                }
                if (file.fail()) {
                    throw failed_operation(__CEFUNCTION__, __LINE__, "Failed to read manifest.");
                }
                //todo: Tell resource manager to unload
                //ResourceMgr::get().unload(releaseList);
            } else {
                throw failed_operation(__CEFUNCTION__, __LINE__, "Failed to open manifest.");
            }
        } else {
            throw invalid_args(__CEFUNCTION__, __LINE__, "Manifest not found. Invalid argument.");
        }
    }
}
