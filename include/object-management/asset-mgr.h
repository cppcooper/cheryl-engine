#pragma once

#include "../../../tools_logger.h"
#include "../STL.h"
#include "../AssetAbstract.h"

class Asset_Manager
{
private:
    logger::Log* m_Log;
    std::map<std::string, GameAssets::AssetObject*> Asset_Table;
protected:
public:
    Asset_Manager();
    ~Asset_Manager();
    bool RecordAsset( std::string AssetName, GameAssets::AssetObject* p );
    bool RemoveRecord( std::string AssetName );
    //TODO: bool RemoveRecord( ManagedObject* p );
    GameAssets::AssetObject* GetAsset( std::string AssetName );
};

