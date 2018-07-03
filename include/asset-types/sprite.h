#pragma once
#ifndef SPRITE_H
#define SPRITE_H

#include "../Components/../AssetAbstract.h"

#include "BasicTypes.h"

namespace CherylE
{
    class SpriteFrame
    {
    private:
        const uint32_t m_FrameIndex;
        const GLuint &m_VAO;
        const GLuint &m_VBO;
        GLSLProgram* &m_Shader;
        Texture* &m_Tex;
        uint32_t m_Width;
        uint32_t m_Height;
        const float& p_Scale; //ref of Sprite::m_Scale (p_ for parent)
        const float& p_Alpha; //ref of Sprite::m_Alpha (p_ for parent)

    public:
        float m_Scale = 1.0f;
        float m_Alpha = 1.0f;
        glm::vec3 m_Offset; //Used to center the frames of a sprite

        SpriteFrame( GLSLProgram* &glslp, Texture* &tex, GLuint &vao, GLuint &vbo, float& global_scale, float& global_alpha,
                     uint32_t frame, uint32_t width, uint32_t height, uint32_t x_offset = 0, uint32_t y_offset = 0, float scale = 1.0f, float alpha = 1.0f );
        uint32_t Width() const { return m_Width; }
        uint32_t Height() const { return m_Height; }
        void Draw( const glm::mat4& matrix );
    };

    class Sprite : public ManagedObject, public VO_Data, public Drawable
    {
    protected:
        std::vector<SpriteFrame> m_Frames;
        uint16_t m_FrameIndex = 0;
        float m_Scale = 1.0f;
        float m_Alpha = 1.0f;

    public:
        uint32_t TypeID();
        void Load( std::string file );
        void Reset();

        SpriteFrame& operator[]( uint16_t frame );
        void Draw() final override { Draw( glm::mat4( 1.f ) ); }
        void Draw( const glm::mat4& matrix ) final override;
        void Scale( float scale );
        void SetAlpha( float alpha );
    };
}

#endif
