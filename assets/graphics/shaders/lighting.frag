#version 330

//layout(binding=0) uniform sampler2D mytexture;
uniform sampler2D mytexture;

in vec2 v_texcoord;
in vec3 Position;
in vec3 Normal;
uniform float in_Alpha;

//lighting variables
uniform vec3 LightPosition;
uniform vec3 LightIntensity;
uniform vec3 Kd; //diffuse reflectivity
uniform vec3 Ka; //ambient reflectivity
uniform vec3 Ks; //Specular reflectivity
uniform float Shininess; //Specular shininess factor

//layout (location = 0) out vec4 FragColor;
out vec4 FragColor;

//ADS Phong model lighting
vec3 ads()
{
	vec3 n = normalize(Normal);
	vec3 s = normalize(LightPosition - Position);
	vec3 v = normalize(vec3(-Position));
	vec3 r = reflect( -s, n);

	return LightIntensity * (Ka + Kd * max( dot(s,n) , 0.0) + Ks * pow( max( dot(r,v), 0.0), Shininess));
}

void main(void)
{	
	vec4 myTexel = 	texture(mytexture, v_texcoord);
	vec4 lighting = vec4(ads(), 1.0);
	
   FragColor = lighting * myTexel * in_Alpha;	
  // FragColor = vec4(1.f, 1.f, 1.f, 0.5f);
}