#include "../../include/resource-management/resources.h"

void ResourceMgr::load(const char* manifest){
    file_mgr.load(manifest);
    for(auto pair : policies){
        if(pair->second.OnLoad()){
            auto iter = file_mgr.find(pair->first);
            auto loader = loaders.find(pair->first).second; //blows up if a loader doesn't exist
            /*todo: because of above add loaders and policies at the same time*/
            while(iter.hasNext()){
                fs::path file = iter.next();
                loadable* resource = loader->load(file);
                if(!loaded.emplace(file.filename(),resource).second){
                    //todo: print log message if filename is duplicate
                }
            }
        }
    }
}