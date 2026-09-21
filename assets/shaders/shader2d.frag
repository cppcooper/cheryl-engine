#version 330 core

uniform sampler2D mytexture;
uniform float in_Alpha;

in vec2 v_texcoord;
out vec4 out_Color;

void main(void)
{	
	vec4 myTexel = texture(mytexture, v_texcoord);

    out_Color = vec4(myTexel.rgb, myTexel.a * in_Alpha);
}
