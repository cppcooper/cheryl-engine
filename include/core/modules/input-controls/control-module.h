#pragma once
#ifndef _INPUT_H_
#define _INPUT_H_

#include "GameModule.h"

#include <mutex>
#include <vector>
#include <map>
#include <functional>


//This class SETS the association between controls and game actions
class ControlModule : public GameModule
{
protected:
    bool m_MouseEnabled;
    struct MState
    {
        double cursor_x;
        double cursor_y;
        int button;
    } mouse_state;
    std::vector<MState> m_MouseInput;
    std::vector<int> m_KeysInput;

    std::mutex m_InputMutex;
    std::mutex m_ActionMutex;
    //TODO: Implement methodology for reconfiguring controls
    std::multimap<int, std::function<void()>> Key_Action_Map;
    std::multimap<int, std::function<void( MState state )>> Button_Action_Map;
    std::vector<std::function<void( double x, double y )>> Cursor_Readers;
    ControlModule(){ m_MouseEnabled = true; }
    ~ControlModule(){}

public:
    static ControlModule& Instance();
    void Disable_MouseInput();
    void Enable_MouseInput();
    void Bind_Cursor( std::function<void( double x, double y )> reader );
    void Bind_MButton( int button, std::function<void( MState state )> action );
    void Bind_Key( int key, std::function<void()> action );

    void MoveCursor( double x, double y );
    void QueueMButton( int button );
    void QueueKey( int key );

    void Process() override;
};

#endif
