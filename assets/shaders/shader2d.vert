#version 330 core

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;
uniform float in_Alpha = 1.f;
uniform float in_Scale = 1.f;

in vec3 in_Position;
in vec2 in_Texcoord; 

out vec2 v_texcoord;
out vec4 gl_Position;


void main(void)
{
	gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(in_Position * in_Scale, 1.0);
	v_texcoord = in_Texcoord;
}