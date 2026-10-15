#include "../Font.h"

#include "../../InterAccess.h"
#include "../../Components/Factory.h"
#include "../../Components/AssetMgr.h"
using namespace GameAssets;


TYPE_ID_IMPL( Font )
void Font::Load( std::string file )
{
    assert( !m_Initialized );

    std::string TexName = file.substr( file.find_last_of( '/' ) + 1, file.find_last_of( '.' ) );// +".png";
    TexName = TexName.substr( TexName.find_last_of( '\\' ) + 1, TexName.find_last_of( '.' ) ) +".png";
    m_Tex = Asset_Factory<Texture>::Instance().GetAsset( TexName );
    assert( m_Tex != nullptr );

    std::ifstream Data;
    Data.open( file, std::ios::in | std::ios::binary );
    assert( Data.is_open() );
    Data.seekg( 0, std::ios::beg );

    short buffer[256]; ///memory to read into
    char* buff = (char*)buffer;
    Data.read( buff, 512 );

    assert( !Data.fail() );
    Data.close();

    for ( int i = 0; i < 256; ++i )
    {
        m_Widths[i] = (int)buffer[i];
    }

    m_Shader = Asset_Factory<GLSLProgram>::Instance().GetAsset( "font_default.glslp" );
    assert( m_Shader != nullptr );

    uint32_t& Tex_width = m_Tex->width;
    uint32_t& Tex_height = m_Tex->height;

    m_vStride = 5 * sizeof( float );
    m_vCount = 4 * 256;
    m_Vertices = new float[m_vCount * 5];
    //Textures are assumed to have 16 characters per line
    for ( int loop = 0; loop<256; loop++ )
    {
        float cx = ( (float)( loop % 16 ) ) / 16.0f;                /// X Position Of Current Character
        float cy = ( (float)( loop / 16 ) ) / 16.0f;                /// Y Position Of Current Character

        m_Vertices[( loop * 20 )] = 0;
        m_Vertices[1 + ( loop * 20 )] = 0;      /// Vertex Coord (Bottom Left)
        m_Vertices[2 + ( loop * 20 )] = 0;
        m_Vertices[3 + ( loop * 20 )] = cx;
        m_Vertices[4 + ( loop * 20 )] = 1 - ( cy + 1.f / 16 );  /// Texture Coord (Bottom Left)
        
        m_Vertices[5 + ( loop * 20 )] = 1;
        m_Vertices[6 + ( loop * 20 )] = 0;  /// Vertex Coord (Bottom Right)
        m_Vertices[7 + ( loop * 20 )] = 0;
        m_Vertices[8 + ( loop * 20 )] = cx + ( 1.f / 16 );
        m_Vertices[9 + ( loop * 20 )] = 1 - ( cy + 1.f / 16 );  /// Texture Coord (Bottom Right)

        m_Vertices[10 + ( loop * 20 )] = 1;
        m_Vertices[11 + ( loop * 20 )] = 1; /// Vertex Coord (Top Right)
        m_Vertices[12 + ( loop * 20 )] = 0;
        m_Vertices[13 + ( loop * 20 )] = cx + ( 1.f / 16 );
        m_Vertices[14 + ( loop * 20 )] = 1 - cy;    /// Texture Coord (Top Right)

        m_Vertices[15 + ( loop * 20 )] = 0;
        m_Vertices[16 + ( loop * 20 )] = 1;     /// Vertex Coord (Top Left)
        m_Vertices[17 + ( loop * 20 )] = 0;
        m_Vertices[18 + ( loop * 20 )] = cx;
        m_Vertices[19 + ( loop * 20 )] = 1 - cy;    /// Texture Coord (Top Left)
    }


    Init();
}

Font::Font()
{
    Reset();
}

void Font::Reset()
{
    m_Angle = 0.0f;
    m_Alpha = 1.0f;
    m_FontSize = 16.0f;
    m_AltFont = false;
    Deinit();
}

float Font::WidthText( std::string text )
{
    int letter;
    float width_text = 0;
    float scale = m_FontSize / 128;

    for ( unsigned int i = 0; i < text.size(); ++i )
    {
        letter = m_AltFont ? text[i] - 32 + 128 : text[i] - 32;

        width_text += m_Widths[letter] * scale;
    }

    return width_text;
}

void Font::Print( float x, float y, std::string text )
{
    m_CursorPos.x = x;
    m_CursorPos.y = y;
    Print( text );
}

void Font::Print( std::string text )
{
    glBindVertexArray( m_VAO );
    m_Tex->Bind();
    m_Shader->UseProgram();

    m_Shader->SetUniform( "in_Alpha", m_Alpha );
    m_Shader->SetUniform( "in_Scale", m_FontSize );
    m_ModelMatrix = glm::translate( glm::mat4( 1.f ), m_CursorPos );
    m_ModelMatrix = glm::rotate( m_ModelMatrix, m_Angle, glm::vec3( 0.f, 0.f, 1.f ) );


    float scale = m_FontSize / 128;
    uint32_t length = (uint32_t)text.size();
    for ( uint32_t i = 0, letter = 0; i < length; ++i )
    {
        m_Shader->SetUniform( "modelMatrix", m_ModelMatrix );
        letter = m_AltFont ? text[i] - 32 + 128 : text[i] - 32;

        if ( text[i] == '\n' )
        {
            m_CursorPos.y -= ( m_FontSize / 2 );
            m_ModelMatrix = glm::translate( glm::mat4( 1.f ), m_CursorPos );
            m_ModelMatrix = glm::rotate( m_ModelMatrix, m_Angle, glm::vec3( 0.f, 0.f, 1.f ) );
        }
        else
        {
            glDrawArrays( GL_QUADS, letter * 4, 4 );
            m_ModelMatrix = glm::translate( m_ModelMatrix, glm::vec3( (float)m_Widths[letter] * scale, 0.f, 0.f ) );
        }
    }
}
