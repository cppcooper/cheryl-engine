#pragma once
#ifndef _ASSETDEFS_H
#define _ASSETDEFS_H

/* AssetDefs.h and ../AssetAbstract.h had to be separated
this ensures Macros from OpenGL_Headers and dirent.h do not collide

***The Asset_Loader needs both the File_Mgr and an Asset interface***

Thus concrete Asset definitions must be seperated from the interface details
*/

#include <string>
#include <map>

#include <glm.hpp>
#include <glfw3.h>

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;



namespace CherylE
{
    /* Fundamental ManagedObject Types can be found below
    
    Last Updated: 2015/10/28
    
    Currently Defined Classes:
======================
    - VO_Data
    - Texture
    - GLSLProgram
    */
    class VO_Data;
    class Texture;
    class GLSLProgram;

    extern bool useMipMaps;

#pragma region "VO_Data"

    class VO_Data
    {
    protected:
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

    public:
        ~VO_Data();
        void Init();
        void Deinit();

    protected:
        unsigned int* m_Indices = nullptr;
        float* m_Vertices = nullptr;
        bool m_Initialized = false;

        GLuint m_iCount = 0; //How many indices
        GLuint m_vCount = 0; //How many vertices
        GLuint m_vStride = 0; //How many bytes in one Triangle/Quad vert

        GLuint m_IndexOrder = 0;
        GLuint m_VBO = 0;
        GLuint m_VAO = 0;
        Texture* m_Tex = nullptr;

    public:
        GLSLProgram* m_Shader = nullptr;
    };

#pragma endregion

#pragma region "Drawable Interface"

    class Drawable
    {
    public:
        virtual void Draw() { Draw( glm::mat4( 1.f ) ); }
        virtual void Draw( const glm::mat4& matrix ) = 0;
    };

#pragma endregion

#pragma region "Texture"

    class Texture : public ManagedObject
    {
    public:
        static uint32_t Texture_Count;
        GLuint glTexture_ID;
        GLuint glTexture_Unit; // 0 - 31
        unsigned int width, height;

        uint32_t TypeID();
        void Load( std::string file );
        void Reset();
        void Bind();
    };

#pragma endregion

#pragma region "GLSLProgram"

    enum GLSLShaderType
    {
        VERTEX, FRAGMENT, GEOMETRY,
        TESS_CONTROL, TESS_EVALUATION
    };

    class GLSLProgram : public ManagedObject
    {
    private:
        GLuint prog_ID;
        GLuint shader_ID[5];
        bool linked;
        std::string log_info;

        std::map<std::string, GLint> Locations;

    protected:
        bool Link();
        GLint GetUniformLocation( const char* name );
        bool compileShaderFromFile( std::string file, GLSLShaderType type );
        bool compileShaderFromString( std::string &source_code, GLSLShaderType type );

    public:
        GLSLProgram();
        ~GLSLProgram();

        uint32_t TypeID();
        void Load( std::string file );
        void Reset();

        std::string GetLog();
        void UseProgram();
        void BindAttributeLocation( GLuint location, const char* name );

        void SetUniform( const char *name, const mat4 & m );
        void SetUniform( const char *name, const mat3 & m );
        void SetUniform( const char *name, const vec4 & v );
        void SetUniform( const char *name, const vec3 & v );
        void SetUniform( const char *name, const vec2 & v );
        void SetUniform( const char *name, float x, float y, float z );
        void SetUniform( const char *name, float x, float y );
        void SetUniform( const char *name, float val );
        void SetUniform( const char *name, int val );
        void SetUniform( const char *name, bool val );

        void PrintActiveUniforms();
        void PrintActiveAttribs();
    };

#pragma endregion

}


#endif