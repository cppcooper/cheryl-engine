#version 330 core

uniform sampler2D mytexture;
uniform float in_Alpha;

in vec2 v_texcoord;
out vec4 out_Color;

void main() {
    vec4 color = texture(mytexture, v_texcoord);
    out_Color = vec4(color.rgb, color.a * in_Alpha);
}
