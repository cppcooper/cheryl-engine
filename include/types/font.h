#pragma once
#ifndef _FONT_HANDLER_H_
#define _FONT_HANDLER_H_


#include "BasicTypes.h"

namespace CherylE
{
    class Font : public ManagedObject, public VO_Data
    {
    protected:
        int m_Widths[256];
        glm::mat4 m_ModelMatrix; /// Store the model matrix
    
    public:
        glm::vec3 m_CursorPos;
        float m_Angle;
        float m_Alpha;
        float m_FontSize;
        bool m_AltFont;

        Font();
        uint32_t TypeID();
        void Load( std::string file );
        void Reset();

        float WidthText( std::string text );
        void Print( float x, float y, std::string text );
        void Print( std::string text );
    };
}

#endif