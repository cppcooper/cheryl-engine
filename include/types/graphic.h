#pragma once
#ifndef GRAPHIC_H
#define GRAPHIC_H

#include "../Components/../AssetAbstract.h"

#include "BasicTypes.h"

namespace CherylE
{
    class Graphic : public ManagedObject, public VO_Data, public Drawable
    {
    protected:
        float m_Scale = 1.0f;
        float m_Alpha = 1.0f;

    public:
        Graphic();
        uint32_t TypeID();
        void Load( std::string file );
        void Reset();

        void Draw() final override { Draw( glm::mat4( 1.f ) ); }
        void Draw( const glm::mat4& matrix ) final override;
        uint16_t Width() const { return (uint16_t)( m_Scale * m_Tex->width ); }
        uint16_t Height() const { return (uint16_t)( m_Scale * m_Tex->height ); }
        void Scale( float scale );
        void SetAlpha( float alpha );
    };
}

#endif