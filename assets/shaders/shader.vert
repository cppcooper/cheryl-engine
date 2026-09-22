#version 330

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;

in vec3 in_Position;
in vec2 in_Texcoord; 
uniform float in_Alpha;


out vec2 v_texcoord;
out vec3 Position;


void main(void)
{
	 Position = vec3(viewMatrix * modelMatrix * vec4(in_Position, 1.0));
     gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(in_Position, 1.0);
     v_texcoord = in_Texcoord;
}