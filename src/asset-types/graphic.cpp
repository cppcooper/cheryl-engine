#include "../Graphic.h"
#include "../../InterAccess.h"
#include "../../Components/Factory.h"
#include "../../Components/AssetMgr.h"
#include "2dtools.h"
using namespace GameAssets;

Graphic::Graphic()
{
    m_Shader = Asset_Factory<GLSLProgram>::Instance().GetAsset( "2d_default.glslp" );
    assert( m_Shader != nullptr );
}

TYPE_ID_IMPL( Graphic )
void Graphic::Load( std::string file )
{
    assert( !m_Initialized );
    m_Tex = Asset_Factory<Texture>::Instance().GetAsset( file );
    ushort width = m_Tex->width;
    ushort height = m_Tex->height;
    m_vCount = 4;
    m_vStride = 5 * sizeof( float );
    m_Vertices = new float[m_vCount * 5];
    AnchorBottomLeft( m_Vertices, width, height, width, height );
    Init();
}

void Graphic::Reset()
{
    m_Scale = 1.0f;
    m_Alpha = 1.0f;
    Deinit();
}

void Graphic::Draw( const glm::mat4& matrix )
{
    glBindVertexArray( m_VAO );
    m_Tex->Bind();
    m_Shader->UseProgram();

    m_Shader->SetUniform( "in_Scale", m_Scale );
    m_Shader->SetUniform( "in_Alpha", m_Alpha );
    m_Shader->SetUniform( "modelMatrix", matrix );

    glDrawArrays( GL_QUADS, 0, 4 );
}

void Graphic::Scale( float scale )
{
    m_Scale = scale; //Apply to Model Matrix
}

void Graphic::SetAlpha( float alpha )
{
    m_Alpha = alpha;
}
