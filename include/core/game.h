#pragma once
#ifndef GAMEFRAMEWORK_H_
/*
**************************
//!Cheryl-Engine - v1.0.2
//!Game Framwork Component.
**************************
The MIT License (MIT)

Copyright (c) <2018> <Josh S. Cooper>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.*/

#define GAMEFRAMEWORK_H_
//******************************************
//!classes: Game
//**********
// Game:
// Composite framework for game development. Includes glEngine & Object_Faculties.
// * Uses glEngine as the basis for working with OpenGL.
// * Uses Object_Faculties to load assets into memory (todo expand on implementation usage).
// * Uses ControlModule to bind game controls, Game is only responsible for binding Escape to Quit. More could be done with the use of a config file.
// * Uses composite pattern with GameModule objects to implement game details.
//******************************************
#ifdef _DEBUG
#pragma comment (lib,"Cheryl-Engine/Debug-GameFramework.lib")
#else
#pragma comment (lib,"Cheryl-Engine/Release-GameFramework.lib")
#endif
//-#pragma comment (lib, "../../redistributables/3rdparty/Steamworks/steam_api.lib")

//-#include "../../3rdparty/Steamworks/public/steam/steam_api.h"
#include "../AssetFaculties/AssetFaculties.h"
#include "../GLEngine/GLEngine.h"
#include "ControlModule.h"
#include "GameModule.h"
#include <vector>

/*Game:
Provides a simple composite framework for building a game. Create GameModule classes to implement various game systems.
Attach GameModule instances to the Game class. Execution of methods are as follows:
* Update => GameModule::[Process, Update, Buffer]
* Draw   => GameModule::[Draw, Output]
//todo devise a way to parallelize more code
X----------------------------------------------X
It will initialize all GameModules attached when it initializes itself, same for deinitializations.
It uses ControlModule to queue input through methods it attachs to glEngine's input callbacks.
It associates file extensions to asset types during initialization, then proceeds to load all assets Object_Faculties can find.
Note: that asset directories should be registered before calling Game::Init()
//todo consider inheritance of glEngine
*/
class Game
{
private:
    //! m_Faculties is essentially useless in this class
    //-Object_Faculties& m_Faculties = Object_Faculties::Instance();
    ControlModule& Controls = ControlModule::Instance();
    glEngine& m_glEngine = glEngine::Instance();

    std::vector<GameModule*> m_Modules;
    bool m_Initialized = false;
    Game(){ Attach( &ControlModule::Instance() ); }

protected:
    void Update( double& );
    void Draw();

    void PressKey( int&, int&, int&, int& );
    void PressMouse( int&, int&, int& );
    void SetCursor( double&, double& );

public:
    static Game& Instance() //todo optional argument
    {
        static Game instance;//todo argument passed to constructor
        return instance;
    }
    //Initializes glEngine and GameModules. Associates file extensions to asset types and loads assets from any currently registered directories.
    void Init();
    //Deinitializes GameModules.
    void Deinit();
    //Attaches GameModules to the Game. Modules are added to a vector of GameModules.
    void Attach( GameModule* );
    //Detaches GameModules from the game, if they are currently attached. Removes from a vector of GameModules.
    void Detach( GameModule* );
    //Performs a single call to glEngine::Run()
    //xxxxxxxxxxxxxxxxxxxx
    //glEngine::Run()
    //!Forks two threads: [Update, Draw]=>callbacks:{Update_f, Draw_f}
    //Main threads continues by polling input
    void Run() const;
};

#endif
