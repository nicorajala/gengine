#version 120

attribute vec3 aPos;
attribute vec3 aColor;
attribute vec3 aNormal;
attribute vec2 aTexCoord;

varying vec3 FragPos;
varying vec3 Color;
varying vec3 Normal;
varying vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    FragPos = vec3(model * vec4(aPos,1.0));
    Color = aColor;
    Normal = aNormal;
    TexCoord = aTexCoord;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}