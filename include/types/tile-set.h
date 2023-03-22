#pragma once
#ifndef TILESET_H
#define TILESET_H
#include <stdafx.h>
#include "../Components/../AssetAbstract.h"
#include "BasicTypes.h"

namespace CherylE
{
    class TileSet : public ManagedObject, public VO_Data, public Drawable
    {
    protected:
        uint16_t m_TileCount = 0;
        uint16_t m_FrameIndex = 0;
        uint16_t m_Width = 0;
        uint16_t m_Height = 0;
        float m_Scale = 1.0f;
        float m_Alpha = 1.0f;

    public:
        uint32_t TypeID();
        void Load( std::string file );
        void Reset();

        TileSet& operator[]( uint16_t frame );
        void Draw( const glm::mat4& matrix );
        void Scale( float scale );
        void SetAlpha( float alpha );

        uint16_t Width() const { return m_Width; }
        uint16_t Height() const { return m_Height; }
    };
}

#endif
