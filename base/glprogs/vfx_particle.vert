#version 120

uniform mat4 uViewProjection;
uniform bool uDepthHack;
varying vec2 vTexCoord;
varying vec4 vColor;

void main() {
	gl_Position = uViewProjection * gl_Vertex;
	if ( uDepthHack ) {
		gl_Position.z = gl_Position.z * 0.3 - gl_Position.w * 0.7;
	}
	vTexCoord = gl_MultiTexCoord0.xy;
	vColor = gl_Color;
}
