#include "../../../tools_bench.h"
#include "../GLEngine.h"
#include <cstdio>


void key_callback( GLFWwindow* window, int key, int scancode, int action, int mods )
{
    glEngine::Instance().key_callback_f( key, scancode, action, mods );
}

void cursor_position_callback( GLFWwindow* window, double x, double y )
{
    glEngine::Instance().cursor_position_callback_f( x, y );
}

void mouse_button_callback( GLFWwindow* window, int button, int action, int mods )
{
    glEngine::Instance().mouse_button_callback_f( button, action, mods );
}

void window_resize_callback( GLFWwindow* window, int width, int height )
{
    glEngine::Instance().Resize( width, height );
}


glEngine::glEngine( GLE_enum::window windowMode, int width, int height )
{
    m_wMode = windowMode;
    m_Screen.m_Width = width;
    m_Screen.m_Height = height;

    m_nearplane = 0.1f;
    m_farplane = 10000.f;
}

void glEngine::UpdatePrjMat()
{
    if ( m_gMode == GLE_enum::graphics::R3D )
    {
        /// Enable Depth Testing for 3D!
        glEnable( GL_DEPTH_TEST );

        ///3D perspective projection
        m_projectionMatrix = glm::mat4( 1.f ) * glm::perspective( 45.0f, (GLfloat)( m_Screen.Width() ) / (GLfloat)( m_Screen.Height() ), m_nearplane, m_farplane );
    }
    else
    {
        /// Disable Depth Testing for 2D!
        glDisable( GL_DEPTH_TEST );

        ///2d orthographic projection
        m_projectionMatrix = glm::mat4( 1.f ) * glm::ortho( 0.f, (float)m_Screen.Width(), 0.f, (float)m_Screen.Height(), 0.f, 1.f );
    }
    //todo Pass new matrix calculation to GLSLProgram(s)?? Pass to all? Pass at all?
}

void glEngine::Update()
{
    time_lord timer;
    timer.set_TimePoint_A();
    timer.set_TimePoint_B();
    double seconds;
    while ( m_RunThreads )
    {
        seconds = timer.Elapsed_seconds();
        timer.set_TimePoint_A();
        Update_f( seconds );
        timer.set_TimePoint_B();
    }
}

void glEngine::Draw()
{
    ///Bring rendering context to this thread
    wglMakeCurrent( m_currentDC, m_currentContext );

    glClearColor( m_red, m_green, m_blue, m_alpha );
    while( m_RunThreads )
    {
        if( m_ManualBufferSwap )
        {
            Draw_f();
        }
        else
        {
            glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
            Draw_f();
            glfwSwapBuffers( m_window );
        }
    }
    ///We need to disassociate this thread with OpenGL's rendering
    wglMakeCurrent( 0, 0 );
}



glEngine::~glEngine()
{
    ///If the threads are currently running then we need to stop them
    if ( m_RunThreads )
    {
        Quit();
    }
}

//todo print error messages, return void
//Method initializes OpenGL for rendering to a window/viewport
bool glEngine::Init()
{
    // checks what features are available on the machine, enumerates monitors and joysticks, initializes the timer and performs any required platform-specific initialization.
    if ( !glfwInit() )
    {
        return false;
    }

    /// Here we query how much sampling is possible and set that to be used if possible
    GLint samples = 8;
    glGetIntegerv( GL_SAMPLES, &samples );
    if ( samples )
    {
        glEnable( GL_MULTISAMPLE );
    }
    glfwWindowHint( GLFW_SAMPLES, samples );

    GLFWmonitor** monitors;
    int count;

    monitors = glfwGetMonitors( &count );

    const GLFWvidmode* mode = glfwGetVideoMode( glfwGetPrimaryMonitor() );

    ///Create a window of a particular type
    switch ( m_wMode )
    {
        case GLE_enum::window::FULLSCREEN:
        {
            m_window = glfwCreateWindow( mode->width, mode->height, "Fullscreen", glfwGetPrimaryMonitor(), NULL );
            m_Screen.m_Height = mode->height;
            m_Screen.m_Width = mode->width;
            break;
        }

        case GLE_enum::window::DECORATEDWINDOW:
        {
            m_window = glfwCreateWindow( m_Screen.Width(), m_Screen.Height(), "Decorated Window", NULL, NULL );
            break;
        }

        case GLE_enum::window::BORDERLESSFULLSCREEN:
        {
            glfwWindowHint( GLFW_DECORATED, false );

            glfwWindowHint( GLFW_RED_BITS, mode->redBits );
            glfwWindowHint( GLFW_GREEN_BITS, mode->greenBits );
            glfwWindowHint( GLFW_BLUE_BITS, mode->blueBits );
            glfwWindowHint( GLFW_REFRESH_RATE, mode->refreshRate );

            m_window = glfwCreateWindow( mode->width, mode->height, "Borderless Fullscreen", NULL, NULL );
            m_Screen.m_Height = mode->height;
            m_Screen.m_Width = mode->width;
            break;
        }
    }

    /// If creating the window failed we need to terminate
    if ( !m_window )
    {
        glfwTerminate();
        return false;
    }
    /// Associates this window with OpenGL's rendering (I believe)
    glfwMakeContextCurrent( m_window );

    /// Sets our input processing function, all input will be passed to this function
    //glfwSetScrollCallback( window, scroll_callback );
    glfwSetKeyCallback( m_window, key_callback );
    glfwSetCursorPosCallback( m_window, cursor_position_callback );
    glfwSetMouseButtonCallback( m_window, mouse_button_callback );
    glfwSetWindowSizeCallback( m_window, window_resize_callback );

    /// start GLEW extension handler
    glewExperimental = GL_TRUE;

    ///Initialize OpenGL functions
    glewInit();
    const GLubyte* renderer = glGetString( GL_RENDERER ); /// get renderer string
    const GLubyte* version = glGetString( GL_VERSION ); /// version as a string
    ///oLog( Level::Info ) << "Renderer: " << renderer;
    ///oLog( Level::Info ) << "OpenGL version supported: " << version;

    //-m_projectionMatrix = glm::mat4( 1.f );
    UpdatePrjMat();
    m_viewMatrix = glm::mat4( 1.f );

    glEnable( GL_CULL_FACE );
    glCullFace( GL_BACK );
    glFrontFace( GL_CCW );

    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    
    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f ); ///clear colour: r,g,b,a    
    glfwSwapInterval( 1 ); ///cap FPS

    m_currentDC = wglGetCurrentDC();
    m_currentContext = wglGetCurrentContext();

    return true;
}

void glEngine::Run()
{
    ///We need to disassociate this thread with OpenGL's rendering
    wglMakeCurrent( 0, 0 );

    if( !Update_f || !Draw_f )
    {
        fprintf( stderr, "Callback(s) not set: Update_f, Draw_f.\n %s - %s:line %d\n", __CONFIG__, __FILE__, __LINE__ );
        abort();
    }
    ///Start our threads
    m_RunThreads = true;
    Game_Update_thread = std::thread( &glEngine::Update, this );
    Game_Draw_thread = std::thread( &glEngine::Draw, this );

    while ( !glfwWindowShouldClose( m_window ) )
    {
        ///main thread does input polling
        glfwPollEvents();

        //todo parameterize thread's sleep duration
        std::this_thread::sleep_for( std::chrono::milliseconds( 75 ) );
    }
    Quit();
    ///Bring the context back to this thread
    wglMakeCurrent( m_currentDC, m_currentContext );
    glfwTerminate();
}

void glEngine::Quit()
{
    if ( m_RunThreads )
    {
        ///Controls threads
        m_RunThreads = false;
        ///Joins threads
        Game_Update_thread.join();
        Game_Draw_thread.join();

        ///Closes window
        glfwSetWindowShouldClose( m_window, GL_TRUE );
    }
}



void glEngine::ShowCursor( bool show )
{
    if ( show )
    {
        glfwSetInputMode( m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL );
    }
    else
    {
        glfwSetInputMode( m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN );
    }
}

void glEngine::ManualBufferSwap( bool enable )
{
    m_ManualBufferSwap = enable;
}

void glEngine::LockScreenSize(bool lock)
{
    m_ScreenSizeLock = lock;
}



void glEngine::SetMode( GLE_enum::graphics newMode )
{
    if ( m_gMode == newMode ) return;
    m_gMode = newMode;
    UpdatePrjMat();
}

void glEngine::set_Update_f( std::function<void( double& seconds )> Update_f )
{
    this->Update_f = Update_f;
}

void glEngine::set_Draw_f( std::function<void()> Draw_f )
{
    this->Draw_f = Draw_f;
}

//todo Add logging(or debug normally) to ensure user resizing doesn't create an infinite cascade of callbacks
void glEngine::Resize( int width, int height )
{
    if ( !m_ScreenSizeLock )
    {
        assert( width >= 0 && height >= 0 );
        const GLFWvidmode * mode = glfwGetVideoMode( glfwGetPrimaryMonitor() );
        
        //Checks that we don't exceed the maximum size
        width = width <= mode->width ? width : mode->width;
        height = height <= mode->height ? height : mode->height;
        
        glfwSetWindowSize( m_window, width, height );
        glViewport( 0, 0, width, height );
        m_Screen.Set( width, height );
        UpdatePrjMat();
    }
}

void glEngine::SetClearColor( float r, float g, float b, float a )
{
    m_red = r;
    m_blue = b;
    m_green = g;
    m_alpha = a;
}
