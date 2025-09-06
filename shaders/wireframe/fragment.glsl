#version 120

varying vec3 FragPos;
varying vec3 Color;
varying vec3 Normal;
varying vec2 TexCoord;

uniform sampler2D uTexture;
uniform bool useTexture;
uniform vec3 overrideColor;
uniform int useOverrideColor;

void main() {
	vec3 baseColor = Color;

	if(useOverrideColor == 1) {
		baseColor = overrideColor;
	}

	gl_FragColor = vec4(baseColor, 1.0);
}