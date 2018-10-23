#pragma once

#include "../../../tools_logger.h"
#include "../STL.h"
#include "../AssetAbstract.h"

#define isderived(base,type) std::enable_if<std::is_base_of<base, type>::value>

class Asset_Manager
{
private:
    logger::Log* m_Log;
    std::map<std::string, GameAssets::iAsset*> Asset_Table;
protected:
public:
    Asset_Manager();
    ~Asset_Manager();
    bool RecordAsset( std::string AssetName, GameAssets::iAsset* p );
    //todo: learn template requirements
    template<class T> //ensure T inherits from GameAssets::iAsset
    vool RecordAsset( std::string AssetName, isderived(GameAssets::iAsset, T)::type*);
    bool RemoveRecord( std::string AssetName );
    //TODO: bool RemoveRecord( ManagedObject* p );
    GameAssets::AssetObject* GetAsset( std::string AssetName );
};

