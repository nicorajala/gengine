#version 120
attribute vec3 aPos;
uniform mat4 view;
uniform mat4 projection;
varying vec3 vWorldDir;

void main() {
    vec4 pos = projection * view * vec4(aPos, 0.0);
    gl_Position = pos.xyww;
    vWorldDir = aPos;
}