#version 330

//layout(binding=0) uniform sampler2D mytexture;
uniform sampler2D mytexture;

in vec2 v_texcoord;
in vec3 Position;
uniform float in_Alpha;

//layout (location = 0) out vec4 FragColor;
out vec4 FragColor;

void main(void)
{	
	vec4 myTexel = 	texture(mytexture, v_texcoord);
	
    FragColor = myTexel * in_Alpha;	
}