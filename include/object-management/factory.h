#pragma once

#include "../../../tools_logger.h"
#include "../STL.h"
#include "../InterAccess.h"
#include "../AssetAbstract.h"
#include "AssetMgr.h"
#include "Pool.h"

//todo: replace ManagedObject with AssetObject
//************
//Asset Factory<T>
//
template<class T>
class Asset_Factory : public GameAssets::Factory
{
private:
    logger::Log* m_Log;
    uint32_t TID = 0;
    Asset_Factory();
    logger::LogStream gLog(logger::LogLevel level);

public:
    ~Asset_Factory();
    static Asset_Factory<T>& Instance();
    bool IsFactoryType(GameAssets::ManagedObject* p);

    static uint32_t TypeID();
    uint32_t Get_TypeID() final override;
    std::string& TypeExtensions() final override;
    std::string& RecordExtension() final override;

    GameAssets::ManagedObject* Create(uint32_t N = 1) final override;
    T* LoadAsset(std::string FileName);
    T* GetAsset(std::string AssetName);
    T* Cast(GameAssets::ManagedObject* p);
};

/*
template<class T>
class Object_Factory : public GameAssets::Factory
{
private:
    logger::Log* m_Log;
    uint32_t TID = 0;
    Object_Factory();
    logger::LogStream gLog(logger::LogLevel level);

public:
    ~Object_Factory();
    static Object_Factory<T>& Instance();
    bool IsFactoryType(GameAssets::ManagedObject* p);

    static uint32_t TypeID();
    uint32_t Get_TypeID() final override;
    std::string& TypeExtensions() final override;
    std::string& RecordExtension() final override;

    GameAssets::ManagedObject* Create(uint32_t N = 1) final override;
    T* Cast(GameAssets::ManagedObject* p);
};*/

//*******************************************
//ASSET FACTORY METHOD IMPLEMENTATIONS
//*******************************************

#pragma region "private"

template<class T>
Asset_Factory<T>::Asset_Factory()
{
    Object_Faculties::Instance().Factories.push_back( ( GameAssets::Factory* )this );
    TID = Object_Faculties::Instance().Factories.size();
    m_Log = &Object_Faculties::GetManagementLog();
    m_Log->Line( _INFO ) << "Factory Initialized #" << TID;
}

template<class T>
logger::LogStream Asset_Factory<T>::gLog(logger::LogLevel level)
{
    LogStream q( m_Log->Line( level ) );
    return q;
}

#pragma endregion


#pragma region "public"

template<class T>
Asset_Factory<T>::~Asset_Factory()
{
    m_Log->Line( _INFO ) << "Factory Deinitialized #" << TID;
}

template<class T>
Asset_Factory<T>& Asset_Factory<T>::Instance()
{
    static Asset_Factory<T> instance;
    return instance;
}

template<class T>
bool Asset_Factory<T>::IsFactoryType(GameAssets::ManagedObject* p)
{
    assert( p != nullptr );
    gLog( _DEBUG1 ) << "Factory ID: " << TID
        << newl << "p: " << p
        << newl << "p Type ID: " << p->TypeID();
    return ( p->TypeID() == TID );
}



//defunct - todo:delete static method
template<class T>
uint32_t Asset_Factory<T>::TypeID()
{
    return Instance().TID;
}

template<class T>
uint32_t Asset_Factory<T>::Get_TypeID()
{
    return TID;
}

template<class T>
std::string& Asset_Factory<T>::TypeExtensions()
{
    static std::string extensions = "";
    return extensions;
}

template<class T>
std::string& Asset_Factory<T>::RecordExtension()
{
    static std::string extension = "";
    return extension;
}



template<class T>
GameAssets::ManagedObject* Asset_Factory<T>::Create(uint32_t N)
{
    gLog(_INFO) << "Factory #" << TID << " Creating Array with " << N << " elements.";
    return (GameAssets::ManagedObject*)Object_Faculties::Instance().Pool->Get<T>(N);
}

template<class T>
T* Asset_Factory<T>::LoadAsset(std::string FileName)
{
    gLog( _INFO ) << "Factory #" << TID << " Loading Asset " << FileName.c_str();
    GameAssets::ManagedObject* p = Object_Faculties::Instance().LoadAsset( TID, FileName );
    return IsFactoryType( p ) ? (T*)p : nullptr;
}

template<class T>
T* Asset_Factory<T>::GetAsset(std::string AssetName)
{
    gLog( _INFO ) << "Factory #" << TID << " Retrieving Asset " << AssetName.c_str();
    GameAssets::ManagedObject* p = Object_Faculties::Instance().Manager->GetAsset( AssetName );
    if ( p != nullptr && IsFactoryType( p ) ){
        return (T*)p;
    }
    return nullptr;
}

template<class T>
T* Asset_Factory<T>::Cast(GameAssets::ManagedObject* p)
{
    if(IsFactoryType(p))
    {
        return (T*)p;
    }
    return nullptr;
}

#pragma endregion


//*******************************************
//OBJECT FACTORY METHOD IMPLEMENTATIONS
//*******************************************