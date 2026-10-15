#include "../BasicTypes.h"

#include "../../Components/Factory.h"
#include "../../../../3rdparty/FreeImage/FreeImage.h"
using namespace GameAssets;

#include <fstream>
#include <sstream>
using std::ifstream;
using std::ios;
using std::ostringstream;


bool GameAssets::useMipMaps = true;


#pragma region "VO_Data Definition"

VO_Data::~VO_Data()
{
    Deinit();
}

void VO_Data::Init()
{
    //TODO: find out if the relevant GLSLProgram must be active before VO_Data::Init()
    if ( m_Initialized )
        return;

    if ( m_vStride < 20 )
        assert( 0 );

    //Triangle or larger?
    if ( m_vCount >= 3 )
    {
        glGenVertexArrays( 1, &m_VAO );
        glBindVertexArray( m_VAO );

        glGenBuffers( 1, &m_VBO );
        glBindBuffer( GL_ARRAY_BUFFER, m_VBO );
        glBufferData( GL_ARRAY_BUFFER, m_vCount * m_vStride, m_Vertices, GL_STATIC_DRAW );
        /*The VAO composition must match what the Shaders expect as input
        What each shader expects depends on whether the shader is 2D or 3D
        2D: X,Y,Z,U,V
        3D: X,Y,Z,Nx,Ny,Nz,U,V

        The code below maps VAO data to Shader input fields
        */
        if ( m_iCount != 0 )
        {
            if ( m_vStride < 32 )
                assert( 0 );

            glEnableVertexAttribArray( 0 ); //XYZ
            glEnableVertexAttribArray( 1 ); //Normal
            glEnableVertexAttribArray( 2 ); //UV
            glBindBuffer( GL_ARRAY_BUFFER, m_VBO );
            glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, m_vStride, BUFFER_OFFSET( 0 ) );
            glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, m_vStride, BUFFER_OFFSET( sizeof( float ) * 3 ) );
            glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, m_vStride, BUFFER_OFFSET( sizeof( float ) * 6 ) );
            glGenBuffers( 1, &m_IndexOrder ); //Triangle Strip Data
            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_IndexOrder );
            glBufferData( GL_ELEMENT_ARRAY_BUFFER, m_iCount * sizeof( int ), m_Indices, GL_STATIC_DRAW );
        }
        else
        {
            glEnableVertexAttribArray( 0 ); //XYZ
            glEnableVertexAttribArray( 1 ); //UV
            glBindBuffer( GL_ARRAY_BUFFER, m_VBO );
            glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, m_vStride, BUFFER_OFFSET( 0 ) );
            glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, m_vStride, BUFFER_OFFSET( sizeof( float ) * 3 ) );
        }
        delete m_Vertices;
        delete m_Indices;
        m_Vertices = nullptr;
        m_Indices = nullptr;
        m_Initialized = true;
        assert( glGetError() == GL_NO_ERROR );
        return;
    }
    assert( 0 );
}

void VO_Data::Deinit()
{
    if ( !m_Initialized )
        return;

    glDeleteBuffers( 1, &m_VBO );
    if ( m_iCount != 0 )
        glDeleteBuffers( 1, &m_IndexOrder );

    glDeleteVertexArrays( 1, &m_VAO );

    m_VAO = 0;
    m_VBO = 0;
    m_IndexOrder = 0;
    m_Tex = nullptr;
    m_Shader = nullptr;
    m_iCount = 0;
    m_vCount = 0;
    m_Initialized = false;
}

#pragma endregion


#pragma region "Texture Definition"

unsigned int Texture::Texture_Count = 0;

TYPE_ID_IMPL( Texture )
void Texture::Load( std::string file )
{
    ///Loaded Images file format
    FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
    FIBITMAP* FreeImage_texture_ptr = nullptr;
    BYTE* image_data = nullptr;
    unsigned int texture_width( 0 ), texture_height( 0 );

    ///File Format from file structure & headers
    fif = FreeImage_GetFileType( file.c_str(), 0 );
    ///Might not have worked
    if ( fif == FIF_UNKNOWN )
    {
        ///File Format from extension
        fif = FreeImage_GetFIFFromFilename( file.c_str() );

        if ( fif == FIF_UNKNOWN )
        {
            ///All attempts to determine format have failed
            assert( 0 );
        }
    }

    ///Verify we can read the format
    if ( FreeImage_FIFSupportsReading( fif ) )
    {
        FreeImage_texture_ptr = FreeImage_Load( fif, file.c_str() );
        if ( !FreeImage_texture_ptr )
        {
            ///Couldn't load the image for unknown reason
            assert( 0 );
        }
    }
    else
    {
        ///We can't read the format so a major problem exists
        assert( 0 );
    }

    if ( FreeImage_GetBPP( FreeImage_texture_ptr ) != 32 )
    {
        ///Converts the image to 32-bit if it wasn't already
        FreeImage_texture_ptr = FreeImage_ConvertTo32Bits( FreeImage_texture_ptr );
    }

    ///Retrieve the image data
    image_data = FreeImage_GetBits( FreeImage_texture_ptr );

    ///Save the Image Dimensions for later use
    texture_width = FreeImage_GetWidth( FreeImage_texture_ptr );
    texture_height = FreeImage_GetHeight( FreeImage_texture_ptr );

    if ( ( image_data == 0 ) || ( texture_width == 0 ) || ( texture_height == 0 ) )
    {
        ///Something went horribly wrong
        assert( 0 );
    }

    ///We need to create an OpenGL texture ID to associate our data to
    glGenTextures( 1, &glTexture_ID );

    ///Bind to the new texture ID
    glActiveTexture( GL_TEXTURE0 ); //GLEW #define
    glBindTexture( GL_TEXTURE_2D, glTexture_ID );

    ///Set up some vars for OpenGL texturizing
    GLenum image_format = GL_RGBA;
    GLint internal_format = GL_RGBA;
    GLint level = 0;

    ///Upload the image data for OpenGL to use
    glTexImage2D( GL_TEXTURE_2D, level, internal_format, texture_width, texture_height, 0, image_format, GL_UNSIGNED_BYTE, image_data );

    ///Need to figure out what this does
    GLint swizzleMask[] = {GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA};
    glTexParameteriv( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask );
    //###########################################################


    ///Do we ask OpenGL to optimize texture scaling or not?
    if ( useMipMaps )
    {
        glGenerateMipmap( GL_TEXTURE_2D );

        FreeImage_Unload( FreeImage_texture_ptr );
        width = texture_width;
        height = texture_height;
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST ); ///for when we are close
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );///when we are far away
    }
    else
    {
        FreeImage_Unload( FreeImage_texture_ptr );
        width = texture_width;
        height = texture_height;
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR ); ///for when we are close
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );///when we are far away
    }

    ///The following turns on Anisotropy
    if ( GL_EXT_texture_filter_anisotropic )
    {
        GLfloat largest_supported_anisotropy;
        glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &largest_supported_anisotropy );
        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, largest_supported_anisotropy );
    }

    /// The texture stops at the edges with GL_CLAMP_TO_EDGE
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

    glTexture_Unit = GL_TEXTURE0;
    return void( 0 );
}

void Texture::Reset()
{
    glDeleteTextures( 1, &glTexture_ID );
    glTexture_ID = 0;
    width = 0;
    height = 0;
}

void Texture::Bind()
{
    glActiveTexture( glTexture_Unit );
    glBindTexture( GL_TEXTURE_2D, glTexture_ID );
}

#pragma endregion


#pragma region "GLSLProgram Definition"

#pragma region "Protected Methods"

bool GLSLProgram::Link()
{
    if ( linked )
    {
        return true;
    }
    else if ( prog_ID <= 0 )
    {
        return false;
    }
    else
    {
        ///Program is not already linked, and is linkable
        glLinkProgram( prog_ID );

        ///Gotta verify the link went Okay
        int status = 0;
        glGetProgramiv( prog_ID, GL_LINK_STATUS, &status );
        if ( GL_FALSE == status )
        {
            ///It failed, we need logs

            ///Get the length of the log
            int length = 0;
            glGetProgramiv( prog_ID, GL_INFO_LOG_LENGTH, &length );

            if ( length > 0 )
            {
                ///The log has a non zero size
                // So allocate space for the log temporarily
                char * c_log = new char[length];
                int written = 0;
                glGetProgramInfoLog( prog_ID, length, &written, c_log );
                log_info = c_log;
                delete[] c_log;
            }
            return false;
        }
        else
        {
            /// It didn't fail
            linked = true;
            return true;
        }
    }
}

int GLSLProgram::GetUniformLocation( const char * name )
{
    UseProgram();
    int result = -1;

    if ( linked )
    {
        auto UMapIter = Locations.find( name );
        if ( UMapIter == Locations.end() )
        {
            result = glGetUniformLocation( prog_ID, name );

            if ( result != -1 ) Locations[name] = result;
        }
        else
        {
            result = UMapIter->second;
        }
    }

    return result;
}

bool GLSLProgram::compileShaderFromFile( std::string file, GLSLShaderType type )
{
    if ( prog_ID == 0 )
    {
        prog_ID = glCreateProgram();
        if ( prog_ID == 0 )
        {
            log_info = "Unable to create shader program.";
            return false;
        }
    }

    ifstream inFile( file, ios::in );

    if ( inFile.is_open() )
    {
        inFile.seekg( 0, inFile.end );
        unsigned int length = (unsigned int)inFile.tellg();
        char* buffer = new char[length + 1];
        memset( buffer, 0, length + 1 );

        inFile.seekg( 0, inFile.beg );
        inFile.read( buffer, length );
        if ( inFile.bad() )
        {
            delete[] buffer;
            log_info = "Unable to read file.";
            return false;
        }
        std::string code( buffer );
        delete[] buffer;
        return compileShaderFromString( code, type );
    }

    log_info = "File not found.";
    return false;
}

bool GLSLProgram::compileShaderFromString( std::string &source_code, GLSLShaderType type )
{
    if ( prog_ID == 0 )
    {
        prog_ID = glCreateProgram();
        if ( prog_ID == 0 )
        {
            log_info = "Unable to create shader program.";
            return false;
        }
    }

    ///Creating the shader handle
    GLuint shader_handle;
    switch ( type )
    {
        case GLSLShaderType::VERTEX:
            shader_handle = glCreateShader( GL_VERTEX_SHADER );
            shader_ID[0] = shader_handle;
            break;
        case GLSLShaderType::FRAGMENT:
            shader_handle = glCreateShader( GL_FRAGMENT_SHADER );
            shader_ID[1] = shader_handle;
            break;
        case GLSLShaderType::GEOMETRY:
            shader_handle = glCreateShader( GL_GEOMETRY_SHADER );
            shader_ID[2] = shader_handle;
            break;
        case GLSLShaderType::TESS_CONTROL:
            shader_handle = glCreateShader( GL_TESS_CONTROL_SHADER );
            shader_ID[3] = shader_handle;
            break;
        case GLSLShaderType::TESS_EVALUATION:
            shader_handle = glCreateShader( GL_TESS_EVALUATION_SHADER );
            shader_ID[4] = shader_handle;
            break;
        default:
            log_info = "Unknown shader type.";
            return false;
    }

    ///Compile the shader
    const char * cstr_code = source_code.c_str();
    glShaderSource( shader_handle, 1, &cstr_code, NULL );
    glCompileShader( shader_handle );

    /// Check for errors
    int result;
    glGetShaderiv( shader_handle, GL_COMPILE_STATUS, &result );
    if ( GL_FALSE == result )
    {
        /// Compile failed, store log and return false
        int length = 0;
        log_info = "";
        glGetShaderiv( shader_handle, GL_INFO_LOG_LENGTH, &length );

        if ( length > 0 )
        {
            char * c_log = new char[length];
            int written = 0;
            glGetShaderInfoLog( shader_handle, length, &written, c_log );
            log_info = c_log;
            delete[] c_log;
        }
        return false;
    }
    else
    {
        /// Compile succeeded, attach glShader to glProgram
        glAttachShader( prog_ID, shader_handle );
        return true;
    }
}

#pragma endregion


#pragma region "Ctor / Dtor"

GLSLProgram::GLSLProgram()
{
    prog_ID = 0;
    shader_ID[0] = 0;
    shader_ID[1] = 0;
    shader_ID[2] = 0;
    shader_ID[3] = 0;
    shader_ID[4] = 0;
    linked = false;
    log_info = "";
}

GLSLProgram::~GLSLProgram()
{
    if ( prog_ID )
    {
        glDeleteProgram( prog_ID );
        glDeleteShader( shader_ID[0] );
        glDeleteShader( shader_ID[1] );
        glDeleteShader( shader_ID[2] );
        glDeleteShader( shader_ID[3] );
        glDeleteShader( shader_ID[4] );
    }
}

#pragma endregion


// The rest are public methods

TYPE_ID_IMPL( GLSLProgram )
void GLSLProgram::Load( std::string file )
{
    size_t pos = file.find_last_of( '.' );
    assert( pos != std::string::npos );

    //TODO: does file_ext include the period? if yes update every file extension below
    std::string file_ext( file.begin() + pos, file.end() );
    std::transform( file_ext.begin(), file_ext.end(), file_ext.begin(), ::tolower );

    if ( file_ext == ".vert" )
    {
        compileShaderFromFile( file, GLSLShaderType::VERTEX );
    }
    else if ( file_ext == ".frag" )
    {
        compileShaderFromFile( file, GLSLShaderType::FRAGMENT );
    }
    else if ( file_ext == ".geom" )
    {
        compileShaderFromFile( file, GLSLShaderType::GEOMETRY );
    }
    else if ( file_ext == ".tesc" )
    {
        compileShaderFromFile( file, GLSLShaderType::TESS_CONTROL );
    }
    else if ( file_ext == ".tese" )
    {
        compileShaderFromFile( file, GLSLShaderType::TESS_EVALUATION );
    }
}

void GLSLProgram::Reset()
{
    if ( prog_ID != 0 )
    {
        glDeleteProgram( prog_ID );
        glDeleteShader( shader_ID[0] );
        glDeleteShader( shader_ID[1] );
        glDeleteShader( shader_ID[2] );
        glDeleteShader( shader_ID[3] );
        glDeleteShader( shader_ID[4] );

        prog_ID = 0;
        shader_ID[0] = 0;
        shader_ID[1] = 0;
        shader_ID[2] = 0;
        shader_ID[3] = 0;
        shader_ID[4] = 0;
        linked = false;
        log_info = "";
        Locations.clear();
    }
}

std::string GLSLProgram::GetLog()
{
    return log_info;
}

void GLSLProgram::UseProgram()
{
    assert( Link() );
    glUseProgram( prog_ID );
}

void GLSLProgram::BindAttributeLocation( GLuint location, const char * name )
{
    glBindAttribLocation( prog_ID, location, name );
}

void GLSLProgram::SetUniform( const char *name, float x, float y )
{
    int loc = GetUniformLocation( name );
    assert( loc >= 0 && "setUniform failed" );
    if ( loc >= 0 )
    {
        glUniform2f( loc, x, y );
    }
}

void GLSLProgram::SetUniform( const char *name, float x, float y, float z )
{
    int loc = GetUniformLocation( name );
    assert( loc >= 0 && "setUniform failed" );
    if ( loc >= 0 )
    {
        glUniform3f( loc, x, y, z );
    }
}

void GLSLProgram::SetUniform( const char *name, const vec2 & v )
{
    this->SetUniform( name, v.x, v.y );
}

void GLSLProgram::SetUniform( const char *name, const vec3 & v )
{
    this->SetUniform( name, v.x, v.y, v.z );
}

void GLSLProgram::SetUniform( const char *name, const vec4 & v )
{
    int loc = GetUniformLocation( name );
    assert( loc >= 0 && "setUniform failed" );
    if ( loc >= 0 )
    {
        glUniform4f( loc, v.x, v.y, v.z, v.w );
    }
}

void GLSLProgram::SetUniform( const char *name, const mat4 & m )
{
    int loc = GetUniformLocation( name );
    assert( loc >= 0 && "setUniform failed" );
    if ( loc >= 0 )
    {
        glUniformMatrix4fv( loc, 1, GL_FALSE, &m[0][0] );
    }
}

void GLSLProgram::SetUniform( const char *name, const mat3 & m )
{
    int loc = GetUniformLocation( name );
    assert( loc >= 0 && "setUniform failed" );
    if ( loc >= 0 )
    {
        glUniformMatrix3fv( loc, 1, GL_FALSE, &m[0][0] );
    }
}

void GLSLProgram::SetUniform( const char *name, float val )
{
    int loc = GetUniformLocation( name );
    assert( loc >= 0 && "setUniform failed" );
    if ( loc >= 0 )
    {
        glUniform1f( loc, val );
    }
}

void GLSLProgram::SetUniform( const char *name, int val )
{
    int loc = GetUniformLocation( name );
    assert( loc >= 0 && "setUniform failed" );
    if ( loc >= 0 )
    {
        glUniform1i( loc, val );
    }
}

void GLSLProgram::SetUniform( const char *name, bool val )
{
    int loc = GetUniformLocation( name );
    assert( loc >= 0 && "setUniform failed" );
    if ( loc >= 0 )
    {
        glUniform1i( loc, val );
    }
}

void GLSLProgram::PrintActiveUniforms() {

    GLint nUniforms, size, location, maxLen;
    GLchar * name;
    GLsizei written;
    GLenum type;

    glGetProgramiv( prog_ID, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLen );
    glGetProgramiv( prog_ID, GL_ACTIVE_UNIFORMS, &nUniforms );

    name = (GLchar *)malloc( maxLen );

    printf( " Location | Name\n" );
    printf( "------------------------------------------------\n" );
    for ( int i = 0; i < nUniforms; ++i ) {
        glGetActiveUniform( prog_ID, i, maxLen, &written, &size, &type, name );
        location = glGetUniformLocation( prog_ID, name );
        printf( " %-8d | %s\n", location, name );
    }

    free( name );
}

void GLSLProgram::PrintActiveAttribs() {

    GLint written, size, location, maxLength, nAttribs;
    GLenum type;
    GLchar * name;

    glGetProgramiv( prog_ID, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength );
    glGetProgramiv( prog_ID, GL_ACTIVE_ATTRIBUTES, &nAttribs );

    name = (GLchar *)malloc( maxLength );

    printf( " Index | Name\n" );
    printf( "------------------------------------------------\n" );
    for ( int i = 0; i < nAttribs; i++ ) {
        glGetActiveAttrib( prog_ID, i, maxLength, &written, &size, &type, name );
        location = glGetAttribLocation( prog_ID, name );
        printf( " %-5d | %s\n", location, name );
    }

    free( name );
}


#pragma endregion

