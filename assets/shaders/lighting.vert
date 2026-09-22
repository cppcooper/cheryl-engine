#version 330

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;

in vec3 in_Position;
in vec3 in_Normal;
in vec2 in_Texcoord; 
uniform float in_Alpha;


out vec2 v_texcoord;
out vec3 Position;
out vec3 Normal;


void main(void)
{
	mat4 ModelViewMatrix = viewMatrix * modelMatrix;
	
	Position = vec3(ModelViewMatrix * vec4(in_Position, 1.0));
	Normal = vec3(ModelViewMatrix * vec4(in_Normal,0.0));	
	v_texcoord = in_Texcoord;

	gl_Position = projectionMatrix * ModelViewMatrix * vec4(in_Position, 1.0);
}