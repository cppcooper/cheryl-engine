#include "../TileSet.h"

#include "../../InterAccess.h"
#include "../../Components/Factory.h"
#include "../../Components/AssetMgr.h"
#include "2dtools.h"
using namespace GameAssets;

TYPE_ID_IMPL( TileSet )
void TileSet::Load( std::string file )
{
    Reset(); // Initializes TileSet member variables
    std::ifstream Data;
    Data.open( file );
    if ( Data.is_open() )
    {
        uint16_t TexLength = 0;
        Data >> TexLength;

        char* TexFile = new char[TexLength + 1];
        memset( TexFile, 0, TexLength + 1 );
        Data.read( TexFile, 1 );
        Data.read( TexFile, TexLength );
        m_Tex = Asset_Factory<Texture>::Instance().GetAsset( TexFile );
        delete[] TexFile;
        m_Shader = Asset_Factory<GLSLProgram>::Instance().GetAsset( "2d_default.glslp" );
        assert( m_Tex != nullptr );
        assert( m_Shader != nullptr );

        uint32_t& TextureWidth = m_Tex->width;
        uint32_t& TextureHeight = m_Tex->height;

        uint16_t Columns = 0, Rows = 0;
        Data >> Columns;
        Data >> Rows;
        Data >> m_Width;
        Data >> m_Height;

        assert( TextureWidth >= (uint32_t)( Columns * m_Width ) );
        assert( TextureHeight >= (uint32_t)( Rows * m_Height ) );

        m_TileCount = Columns * Rows;
        m_vCount = 4 * m_TileCount;
        m_vStride = 5 * sizeof( float );
        m_Vertices = new float[5 * m_vCount];
        memset( m_Vertices, 0, m_vCount * m_vStride );

        for ( uint32_t row = 0; row < Rows; ++row )
        {
            for ( uint32_t col = 0; col < Columns; ++col )
            {
                AnchorBottomLeft( &m_Vertices[m_FrameIndex++ * 20], TextureWidth, TextureHeight, m_Width, m_Height, col * m_Width, row * m_Height );
            }
        }

        assert( !Data.fail() );
        Data.close();
    }
    else
    {
        assert( 0 ); //Couldn't open file
    }

    Init();
    m_FrameIndex = 0;
}

void TileSet::Reset()
{
    m_TileCount = 0;
    m_FrameIndex = 0;
    m_Width = 0;
    m_Height = 0;
    m_Scale = 1.0f;
    m_Alpha = 1.0f;
    Deinit();
}

TileSet& TileSet::operator[]( uint16_t frame )
{
    m_FrameIndex = frame % m_TileCount;
    return *this;
}

void TileSet::Draw( const glm::mat4& matrix )
{
    glBindVertexArray( m_VAO );
    m_Tex->Bind();
    m_Shader->UseProgram();

    m_Shader->SetUniform( "in_Scale", m_Scale );
    m_Shader->SetUniform( "in_Alpha", m_Alpha );
    m_Shader->SetUniform( "modelMatrix", matrix );

    glDrawArrays( GL_QUADS, m_FrameIndex * 4, 4 );
}

void TileSet::Scale( float scale )
{
    m_Scale = scale;
}

void TileSet::SetAlpha( float alpha )
{
    m_Alpha = alpha;
}