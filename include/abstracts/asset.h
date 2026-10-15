#pragma once
#ifndef CHERYL_ENGINE_ASSET_H
#define CHERYL_ENGINE_ASSET_H

#pragma once

#include <string>
#include <cinttypes>

class ManagedObject_Storage;
class Asset_Loader;
class ManagedObject;

struct Storage_Data {
    ManagedObject* allocation = nullptr;
    size_t index = 0;
    size_t length = 0;
    size_t bytes = 0;
};

class ManagedObject {
protected:
    // todo: replace macros
#define TYPE_ID_IMPL(TYPE) unsigned int TYPE::TypeID() { static unsigned int id = Object_Factory<TYPE>::Instance().Get_TypeID(); return id; }
#define TYPE_ID_IMPL_INLINE(TYPE) unsigned int TypeID() { static unsigned int id = Object_Factory<TYPE>::Instance().Get_TypeID(); return id; }

public:
    virtual ~ManagedObject() {}
    virtual uint32_t TypeID() = 0;
    virtual void Reset() = 0;
    const Storage_Data &GetStorageData() const { return storage; }
    void SetStorageData(const Storage_Data &data) { storage = data; }

protected:
    Storage_Data storage;
};

class AssetObject : public ManagedObject {
public:
    virtual void Load(std::string filename) = 0;
};

#endif //CHERYL_ENGINE_ASSET_H
