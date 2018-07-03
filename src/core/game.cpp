#include "../GameFramework.h"



void Game::Update( double& seconds )
{
    for ( auto m : m_Modules )
    {
        m->Process();
    }
    
    for ( auto m : m_Modules )
    {
        m->Update( seconds );
    }

    //todo parallelize buffering from game logic and/or draw routine(s)
    for ( auto m : m_Modules )
    {
        m->Buffer();
    }
}

void Game::Draw()
{
    for ( auto m : m_Modules )
    {
        m->Draw();
        m->Output();
    }
}

void Game::PressKey( int& key, int& scancode, int& action, int& mod )
{
    if ( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS )
    {
        m_glEngine.Quit();
    }
    else if ( action == GLFW_PRESS )
    {
        Controls.QueueKey( key );
    }
}

void Game::PressMouse( int& button, int& action, int& mod )
{
    if ( action == GLFW_PRESS )
    {
        Controls.QueueMButton( button );
    }
}

void Game::SetCursor( double& x, double& y )
{
    if ( x >= 0 && y >= 1 )
    {
        if ( x < Screen::Width() && y < Screen::Height() )
        {
            Controls.MoveCursor( x, Screen::Height() - y );
        }
    }
}



void Game::Init()
{
    using namespace std::placeholders;
    m_glEngine.set_Update_f( std::bind( &Game::Update, this, _1 ) );
    m_glEngine.set_Draw_f( std::bind( &Game::Draw, this ) );
    m_glEngine.key_callback_f = std::bind( &Game::PressKey, this, _1, _2, _3, _4 );
    m_glEngine.mouse_button_callback_f = std::bind( &Game::PressMouse, this, _1, _2, _3 );
    m_glEngine.cursor_position_callback_f = std::bind( &Game::SetCursor, this, _1, _2 );
    m_glEngine.Init();

    //Configure
    Controls.Bind_Key( GLFW_KEY_ESCAPE, std::bind( &glEngine::Quit, &m_glEngine ) );
    //!Note: The following line ordering matters for the Auto-Load procedure
    using namespace GameAssets;
    Asset_Factory<Texture>::Instance().TypeExtensions() = ".png";
    Asset_Factory<GLSLProgram>::Instance().TypeExtensions() = ".vert;.frag;.geom;.tese;.tesc";
    Asset_Factory<GLSLProgram>::Instance().RecordExtension() = ".glslp";
    Asset_Factory<TileSet>::Instance().TypeExtensions() = ".tdat";
    Asset_Factory<Sprite>::Instance().TypeExtensions() = ".sdat";
    Asset_Factory<Font>::Instance().TypeExtensions() = ".fdat";
    Asset_Factory<Mesh>::Instance().TypeExtensions() = ".s3d";

    /// Camera Data
    /*View_Angle = glm::rotate( View_Angle, 180.0f, glm::vec3( 0, 0, 1 ) );
    View_Matrix = View_Angle * View_Position;
    glm::vec4 dir = View_Angle * glm::vec4( 0, 0, 1, 0 );
    player_straight_axis = glm::vec3( dir.x, dir.y, dir.z );
    dir = View_Angle * glm::vec4( 1, 0, 0, 0 );
    player_side_axis = glm::vec3( dir.x, dir.y, dir.z );*/

    //Load
    Object_Faculties::Instance().LoadAssets();

    //-SteamAPI_Init();
    for ( auto m : m_Modules )
    {
        m->Init();
    }
    
    //OpenGL Stuff?
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    
    m_Initialized = true;
}

void Game::Deinit()
{
    //SteamAPI_Shutdown();
    for( auto it = m_Modules.rbegin(); it != m_Modules.rend(); ++it )
    {
        (*it)->Deinit();
    }
}

void Game::Attach( GameModule* module )
{
    m_Modules.push_back( module );
}

void Game::Detach( GameModule* module )
{
    auto it = m_Modules.begin();
    for ( auto m : m_Modules )
    {
        if ( m == module )
        {
            m_Modules.erase( it );
            return;
        }
        it++;
    }
}

void Game::Run() const
{
    assert( m_Initialized );
    m_glEngine.Run();
}

