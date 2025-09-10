#version 120

attribute vec3 aPos;
varying vec3 TexCoords;

uniform mat4 view;
uniform mat4 projection;

void main() {
    mat3 rotView = mat3(view); // remove translation
    vec3 pos = aPos;
    TexCoords = pos;
    vec4 worldPos = projection * vec4(rotView * pos, 1.0);
    gl_Position = worldPos.xyww;
}