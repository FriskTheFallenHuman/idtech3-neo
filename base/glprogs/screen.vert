#version 120

varying vec4 vColor;

void main() {
	gl_Position = vec4( gl_Vertex.xy, 0.0, 1.0 );
	vColor = gl_Color;
}
