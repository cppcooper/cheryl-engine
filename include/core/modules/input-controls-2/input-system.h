#pragma once
#include <vector>
#include <functional>

//#include "../../../3rdparty/Steamworks/public/steam/steam_api.h"
#pragma comment (lib, "3rdparty/Steamworks/steam_api.lib")

#include "../GameModule.h"
#include "InputContext.h"

/* todo: replace control module? This is multi-user input. */
class InputMapper;
class InputSystem : public GameModule
{
private:
    //uint32 m_ActiveControllers = 0;
    //ControllerHandle_t* m_SteamControllers = nullptr;

    InputContext m_ActiveContext;
    std::vector<InputMapper*> m_Mappers;
protected:
public:
    void QueueAction( std::function<void()> action ) {}
    void QueueRange( std::function<void( double, double )> range ) {}
    void Poll();
    void Init() final override;
    void Process() final override;
};