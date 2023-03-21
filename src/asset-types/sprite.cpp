#include "../Sprite.h"

#include "../../InterAccess.h"
#include "../../Components/Factory.h"
#include "../../Components/AssetMgr.h"
#include "2dtools.h"
using namespace GameAssets;

SpriteFrame::SpriteFrame( GLSLProgram* &glslp, Texture* &tex, GLuint &vao, GLuint &vbo, float& global_scale, float& global_alpha, uint32_t frame, uint32_t width, uint32_t height, uint32_t x_offset, uint32_t y_offset, float scale, float alpha ) :
m_Shader( glslp ),
m_Tex( tex ),
m_VAO( vao ),
m_VBO( vbo ),
p_Scale( global_scale ),
p_Alpha( global_alpha ),
m_FrameIndex( frame )
{
    m_Width = width;
    m_Height = height;
    m_Offset.x = (float)x_offset;
    m_Offset.y = (float)y_offset;
    m_Scale = scale;
    m_Alpha = alpha;
}

void SpriteFrame::Draw( const glm::mat4& matrix )
{
    glm::mat4 model_matrix = glm::translate( matrix, m_Offset );
    glBindVertexArray( m_VAO );
    m_Tex->Bind();
    m_Shader->UseProgram();

    m_Shader->SetUniform( "in_Scale", m_Scale * p_Scale );
    m_Shader->SetUniform( "in_Alpha", m_Alpha * p_Alpha );
    m_Shader->SetUniform( "modelMatrix", model_matrix );

    glDrawArrays( GL_QUADS, m_FrameIndex * 4, 4 );
}

TYPE_ID_IMPL( Sprite )
void Sprite::Load( std::string file )
{
    assert( !m_Initialized );
    std::ifstream Data;
    Data.open( file );
    if ( Data.is_open() )
    {
        uint16_t Sections = 0;
        uint16_t TexLength = 0;

        Data >> Sections; //Total Sections of Frames
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


        uint32_t& Tex_width = m_Tex->width;
        uint32_t& Tex_height = m_Tex->height;
        uint16_t Frames = 0, width = 0, height = 0, x_offset = 0, y_offset = 0;
        float scale = 0.0f, alpha = 0.0f;
        struct frame_data
        {
            uint16_t width;
            uint16_t height;
            uint16_t start_x;
            uint16_t start_y;
            frame_data(){}
            frame_data( uint16_t a, uint16_t b, uint16_t c, uint16_t d ) :
                width( a ), height( b ), start_x( c ), start_y( d ){}
        };
        std::vector<frame_data> frameData;

        m_FrameIndex = 0;
        for ( int i = 0; i < Sections; ++i )
        {
            //Data per Section
            Data >> Frames; //Total Frames
            Data >> width; //Frame Width of current section
            Data >> height; //Frame Height of current section
            Data >> scale;
            Data >> alpha;

            Data >> x_offset; //Frame X Offset
            Data >> y_offset; //Frame Y Offset
            for ( int f = 0; f < Frames; ++f )
            {
                uint16_t x, y;
                Data >> x;
                Data >> y;
                frameData.emplace_back( width, height, x, y );
                SpriteFrame frame( m_Shader, m_Tex,
                                   m_VAO, m_VBO,
                                   m_Scale, m_Alpha,
                                   m_FrameIndex++,
                                   width, height,
                                   x_offset, y_offset,
                                   scale, alpha );
                m_Frames.push_back( frame );
            }
        }

        assert( frameData.size() == m_Frames.size() );
        m_vCount = 4 * m_Frames.size();
        m_vStride = 5 * sizeof( float );
        m_Vertices = new float[m_vCount * 5];
        memset( m_Vertices, 0, m_vCount * m_vStride );
        for ( uint32_t f = 0; f < m_Frames.size(); ++f )
        {
            frame_data& ref = frameData.at( f );

            AnchorCenter( &m_Vertices[f * 20], Tex_width, Tex_height, ref.width, ref.height, ref.start_x, ref.start_y );
        }
        frameData.resize( 0 );
    }
    else
    {
        assert( 0 ); //Couldn't open file
    }
    assert( !Data.fail() );
    Data.close();

    Init();
    m_FrameIndex = 0;
}

void Sprite::Reset()
{
    m_FrameIndex = 0;
    m_Scale = 1.0f;
    m_Alpha = 1.0f;
    Deinit();
}

SpriteFrame& Sprite::operator[]( uint16_t frame )
{
    m_FrameIndex = frame % m_Frames.size();
    return m_Frames.at( m_FrameIndex );
}

void Sprite::Draw( const glm::mat4& matrix )
{
    assert( !m_Frames.empty() );

    m_Frames[m_FrameIndex].Draw( matrix );
}

void Sprite::Scale( float scale )
{
    m_Scale = scale; //Apply to Model Matrix
}

void Sprite::SetAlpha( float alpha )
{
    m_Alpha = alpha;
}
