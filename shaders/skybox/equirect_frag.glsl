#version 120
varying vec3 vWorldDir;
uniform sampler2D equirectMap;

void main() {
    vec3 dir = normalize(vWorldDir);
    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * 3.14159265);
    float v = 0.5 - asin(dir.y) / 3.14159265;
    gl_FragColor = texture2D(equirectMap, vec2(u, v));
}