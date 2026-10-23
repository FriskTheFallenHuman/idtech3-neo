#version 120
varying vec2 vUV;
void main() {
    gl_Position = vec4(gl_Vertex.xy,0.0,1.0);
    vUV = gl_Vertex.xy*.5+.5;
}
