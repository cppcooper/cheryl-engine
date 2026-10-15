#pragma once

#include "../../tools_logger.h"
#include "STL.h"

class Object_Storage;
class Object_Pool;
template<class T>
class Asset_Factory;
class Asset_Loader;
class Asset_Manager;
namespace GameAssets{ class Factory; class ManagedObject; }

class Object_Faculties
{
    friend class Object_Pool;
    template<class T>
    friend class Asset_Factory;
    //todo implement the following
    //template<class T>
    //friend class Object_Factory;
    friend class Asset_Loader;
    friend class Asset_Manager;
private:
    Asset_Manager* Manager = nullptr;
    Asset_Loader* Loader = nullptr;
    std::vector<GameAssets::Factory*> Factories;
    Object_Pool* Pool = nullptr;
    Object_Storage* Allocator = nullptr;

    Object_Faculties();
    ~Object_Faculties();

public:
    static Object_Faculties& Instance();
    
    void Update( double& seconds );
    void RegisterAssetPath( std::string path );
    void ReturnPoolObject( GameAssets::ManagedObject* ptr );
    void LoadAssets();
    GameAssets::ManagedObject* LoadAsset( unsigned int type_ID, std::string FileName );
    static logger::Log& GetManagementLog();

    template<class T>
    T* LoadAsset( std::string FileName )
    {
        return Asset_Factory<T>::Instance().LoadAsset( FileName );
    }
    template<class T>
    T* GetAsset( std::string AssetName )
    {
        return Asset_Factory<T>::Instance().GetAsset( AssetName );
    }
};

