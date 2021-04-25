#pragma once
#include <unordered_map>
#include "../internals.h"
#include "../interfaces/loader.h"
#include "../interfaces/loadable.h"
#include "disk.h"
namespace CherylE {
	class ResourceMgr : public Singleton<ResourceMgr> {
	private:
		FileMgr &file_mgr = Singleton<FileMgr>::get();
		umap<FileExt, Policy*> policies;
		umap<FileExt, Loader*> loaders;
		umap<fs::path, loadable*> loaded; //mapped according to file name, unloaded based on path.. mapping replaced if duplicate name exists

	public:
		bool addType(const FileExt&, Policy*, Loader*);
		void load(const char*);
		void unload(const char*);
		loadable* retrieve(fs::path&);
	};
}