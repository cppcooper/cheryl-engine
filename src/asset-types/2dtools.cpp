#include "2dtools.h"

void AnchorBottomLeft( float* vertices, ushort png_width, ushort png_height, ushort frame_width, ushort frame_height, ushort frame_start_tx, ushort frame_start_ty )
{
    ///Bottom Left
    vertices[0] = 0.0f;
    vertices[1] = 0.0f;
    vertices[2] = 0.0f;
    vertices[3] = (float)frame_start_tx / (float)png_width; 
    vertices[4] = 1.0f - ( ( (float)frame_start_ty + (float)frame_height ) / (float)png_height );

    ///Bottom Right
    vertices[5] = (float)frame_width;
    vertices[6] = 0.0f;
    vertices[7] = 0.0f;
    vertices[8] = ( (float)frame_start_tx + (float)frame_width ) / (float)png_width;
    vertices[9] = 1.0f - ( ( (float)frame_start_ty + (float)frame_height ) / (float)png_height );

    ///Top Right
    vertices[10] = (float)frame_width;
    vertices[11] = (float)frame_height;
    vertices[12] = 0.0f;
    vertices[13] = ( (float)frame_start_tx + (float)frame_width ) / (float)png_width;
    vertices[14] = 1.0f - ( (float)frame_start_ty / (float)png_height );

    ///Top Left
    vertices[15] = 0.0f;
    vertices[16] = (float)frame_height;
    vertices[17] = 0.0f;
    vertices[18] = (float)frame_start_tx / (float)png_width;
    vertices[19] = 1.0f - ( (float)frame_start_ty / (float)png_height );
}


void AnchorCenter( float* vertices, ushort png_width, ushort png_height, ushort frame_width, ushort frame_height, ushort frame_start_tx, ushort frame_start_ty )
{
    ///Bottom Left
    vertices[0] = 0 - ( frame_width / 2.0f );                                                                       /// X
    vertices[1] = 0 - ( frame_height / 2.0f );                                                                  /// Y
    vertices[2] = 0.0f;                                                                                     /// Z
    vertices[3] = (float)frame_start_tx / (float)png_width;                                                 /// U - Texture mapping
    vertices[4] = 1.0f - ( ( (float)frame_start_ty + (float)frame_height ) / (float)png_height );       /// V - Texture mapping (Inverted Axis?)

    ///Bottom Right
    vertices[5] = 0 + ( frame_width / 2.0f );
    vertices[6] = 0 - ( frame_height / 2.0f );
    vertices[7] = 0.0f;
    vertices[8] = ( (float)frame_start_tx + (float)frame_width ) / (float)png_width;
    vertices[9] = 1.0f - ( ( (float)frame_start_ty + (float)frame_height ) / (float)png_height );

    ///Top Right
    vertices[10] = 0 + ( frame_width / 2.0f );
    vertices[11] = 0 + ( frame_height / 2.0f );
    vertices[12] = 0.0f;
    vertices[13] = ( (float)frame_start_tx + (float)frame_width ) / (float)png_width;
    vertices[14] = 1.0f - ( (float)frame_start_ty / (float)png_height );

    ///Top Left
    vertices[15] = 0 - ( frame_width / 2.0f );
    vertices[16] = 0 + ( frame_height / 2.0f );
    vertices[17] = 0.0f;
    vertices[18] = (float)frame_start_tx / (float)png_width;
    vertices[19] = 1.0f - ( (float)frame_start_ty / (float)png_height );
}