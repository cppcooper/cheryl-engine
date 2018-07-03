#pragma once
#ifndef ENGINE3D_H
#ifdef _WIN32
    #define WIN32_EXTRA_LEAN //todo figure out what the fuck this is?
    #include <Windows.h>
#endif
/*
**************************
//!Cheryl-Engine - v1.0.2
//!OpenGL Engine Component.
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

#define ENGINE3D_H
//************************************************************
//!classes: GLEngine | Screen | -input callback functions
//**********
// glEngine:
// Barebones openGL framework, does the initialization and setup for you.
// Also calculates projection matrices for you.
//
// Uses a 3 thread model
// * Main Thread: [input]
// * Children Threads: [Update, Draw]
//************************************************************
#ifdef _DEBUG
#pragma comment (lib,"Cheryl-Engine/Debug-GLEngine.lib")
#else
#pragma comment (lib,"Cheryl-Engine/Release-GLEngine.lib")
#endif

#include "../../3rdparty/OpenGL_Headers.h"
#include <functional>
#include <thread>

namespace GLE_enum {
    enum class graphics { R2D = 0, R3D };
    enum class window { DECORATEDWINDOW = 1, FULLSCREEN, BORDERLESSFULLSCREEN };
}

/*Screen:
Represents the rendering screen. This singleton provides an interface through which glEngine can easily share information.
Global methods:
- static int Width()
- static int Height()
*/
class Screen
{
    friend class glEngine;
private:
    int m_Width = 0;
    int m_Height = 0;
    Screen(){}
    void Set( int width, int height ){
        m_Width = width;
        m_Height = height;
    }
protected:
    static Screen& Instance(){
        static Screen instance;
        return instance;
    }
public:
    static int Width() {return Instance().m_Width;}
    static int Height() {return Instance().m_Height;}
};

/*glEngine:
Provides barebones openGL framework. With 3 threads: Main: [Input]; Children:[Update, Draw]
Both 3d/2d modes available, projection matrix calculations built in.
Uses callbacks to call Update & Draw.
- set_Draw_f(std::function<void()>)
- set_Update_f(std::function<void(double&)>)
public callbacks:
- std::function<void( int& key, int& scancode, int& action, int& mod )> key_callback_f;
- std::function<void( int& button, int& action, int& mod )> mouse_button_callback_f;
- std::function<void( double& x, double& y )> cursor_position_callback_f;
//todo create a buffer thread for draw calls
*/
class glEngine
{
    friend class Screen;
private:
    HDC   m_currentDC;
    HGLRC m_currentContext;
    GLE_enum::graphics m_gMode;
    GLE_enum::window   m_wMode;    

    std::thread                    Game_Update_thread;
    std::thread                    Game_Draw_thread;
    std::function<void( double& )> Update_f;
    std::function<void()>          Draw_f;

public: //todo comment public members
    std::function<void( int&, int&, int&, int& )> key_callback_f;
    std::function<void( int&, int&, int& )>       mouse_button_callback_f;
    std::function<void( double&, double& )>       cursor_position_callback_f;

    GLFWwindow* m_window;
    float       m_nearplane;
    float       m_farplane;
    glm::mat4   m_projectionMatrix;
    glm::mat4   m_viewMatrix;

private:
    Screen&  m_Screen = Screen::Instance();
    bool     m_RunThreads       = false;
    bool     m_ManualBufferSwap = false;
    bool     m_ScreenSizeLock   = false;
    GLclampf m_red   = 0.8f;
    GLclampf m_blue  = 0.6f;
    GLclampf m_green = 0.7f;
    GLclampf m_alpha = 0.0f;

protected:
    glEngine( GLE_enum::window = GLE_enum::window::DECORATEDWINDOW, int = 1920, int = 1080 );
    void UpdatePrjMat();
    void Update();
    void Draw();

public: 
    static glEngine& Instance()
    {
        static glEngine engine( GLE_enum::window::DECORATEDWINDOW, 1000, 600 );
        return engine;
    }
    ~glEngine();

    //!Creates a window and initializes OpenGL to use it. See implementation for more details.
    bool Init();
    //!Forks two threads: [Update, Draw]=>callbacks:{Update_f, Draw_f}
    //Main threads continues by polling input
    void Run();
    //!Joins two threads: [Update, Draw]
    //After which the window and render context will both terminate.
    void Quit();

    //Toggles whether the cursor is shown inside the render window
    void ShowCursor( bool );
    //Toggles whether the Draw thread will automatically swap buffers or not
    void ManualBufferSwap( bool = true );
    //Toggles whether the screen can be resized
    void LockScreenSize( bool = true );

    //Set graphical mode, will affect public member glEngine::m_projectionMatrix
    void SetMode( GLE_enum::graphics );
    //Set callback function for draw routines
    void set_Draw_f( std::function<void()> );
    //Set callback function for game logic execution
    void set_Update_f( std::function<void( double& )> );
    //Resizes the render window
    void Resize( int, int );
    //Sets the colour to use when clearing the screen
    void SetClearColor( float, float, float, float = 0.0f );


};

//? Could these be made externally useful?
////extern void key_callback( GLFWwindow* window, int key, int scancode, int action, int mods );
////extern void mouse_button_callback( GLFWwindow* window, int button, int action, int mods );
////extern void cursor_position_callback( GLFWwindow* window, double x, double y );
////extern void window_resize_callback( GLFWwindow* window, int width, int height );

#endif