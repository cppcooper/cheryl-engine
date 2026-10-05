#version 330 core

uniform sampler2D mytexture;

in vec2 v_texcoord;
in vec4 v_color;
out vec4 out_Color;

void main()
{
    out_Color = texture(mytexture, v_texcoord) * v_color;
}
