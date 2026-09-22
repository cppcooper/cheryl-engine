#version 330 core

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;
uniform float in_Scale = 1.0;

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec2 in_Texcoord;

out vec2 v_texcoord;


void main(void)
{
	gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(in_Position * in_Scale, 1.0);
	v_texcoord = in_Texcoord;
}
