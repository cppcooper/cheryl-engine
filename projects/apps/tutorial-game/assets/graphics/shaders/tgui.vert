#version 330 core

uniform mat4 projectionMatrix;

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_Texcoord;
layout(location = 2) in vec4 in_Color;

out vec2 v_texcoord;
out vec4 v_color;

void main()
{
    gl_Position = projectionMatrix * vec4(in_Position, 1.0);
    v_texcoord = in_Texcoord;
    v_color = in_Color;
}
