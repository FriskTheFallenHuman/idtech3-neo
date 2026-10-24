#version 120

uniform sampler2D uScene;
uniform sampler2D uGammaTable;
uniform vec2 uScreenSize;

void main() {
	vec2 uv = gl_FragCoord.xy / uScreenSize;
	vec3 color = texture2D( uScene, uv ).rgb;
	// Sample the 256 entry lookup table at texel centers; NEAREST filtering
	// keeps every framebuffer value on its exact table entry.
	vec3 entry = color * ( 255.0 / 256.0 ) + 0.5 / 256.0;
	vec3 corrected = vec3(
		texture2D( uGammaTable, vec2( entry.r, 0.5 ) ).r,
		texture2D( uGammaTable, vec2( entry.g, 0.5 ) ).r,
		texture2D( uGammaTable, vec2( entry.b, 0.5 ) ).r );
	gl_FragColor = vec4( corrected, 1.0 );
}
