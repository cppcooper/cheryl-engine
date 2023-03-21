#include "../Mesh.h"

#include "../../InterAccess.h"
#include "../../Components/Factory.h"
#include "../../Components/AssetMgr.h"
using namespace GameAssets;


TYPE_ID_IMPL( Mesh )
void Mesh::Load( std::string file )
{
    std::ifstream Data;
    Data.open( file );
    assert( Data.is_open() );

    m_vStride = 8 * sizeof( float );
    Data >> m_vCount;
    
    uint32_t vFloats = m_vCount * 8;
    m_Vertices = new float[vFloats];
    for ( uint32_t i = 0; i < vFloats; ++i )
    {
        Data >> m_Vertices[i];
    }

    Data >> m_iCount;

    m_Indices = new unsigned int[m_iCount];
    for ( uint32_t i = 0; i < m_iCount; ++i )
    {
        Data >> m_Indices[i];
    }

    std::string TexName = "";
    Data >> TexName;
    m_Tex = Asset_Factory<Texture>::Instance().GetAsset( TexName );
    assert( m_Tex != nullptr );
    assert( !Data.fail() );
    Data.close();

    m_Shader = Asset_Factory<GLSLProgram>::Instance().GetAsset( "3d_default.glslp" );
    assert( m_Shader != nullptr );

    Init();
}

void Mesh::Reset()
{
    m_TriStrip = true;
    Deinit();
}

void Mesh::Draw()
{
    Draw( glm::mat4( 1.f ) );
}

void Mesh::Draw( const glm::mat4& matrix )
{
    glBindVertexArray( m_VAO );
    m_Tex->Bind();
    m_Shader->UseProgram();
    m_Shader->SetUniform( "modelMatrix", matrix );

    glDrawElements( m_TriStrip ? GL_TRIANGLE_STRIP : GL_TRIANGLES, m_iCount, GL_UNSIGNED_INT, BUFFER_OFFSET( 0 ) );
}