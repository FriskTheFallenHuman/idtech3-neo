#version 120

uniform sampler2D uParticle;
uniform sampler2D uSceneDepth;
uniform vec2 uScreenSize;
uniform vec2 uSceneSize;
uniform vec2 uProjectionDepth;
uniform float uSoftDistance;
uniform bool uAdditive;
uniform bool uDepthHack;
varying vec2 vTexCoord;
varying vec4 vColor;

float viewDepth( float depth ) {
	return uProjectionDepth.y / ( depth * 2.0 - 1.0 + uProjectionDepth.x );
}

void main() {
	vec2 uv = gl_FragCoord.xy / uScreenSize;
	// Use the closest opaque sample covered by this low-resolution pixel.
	// This prevents coarse particles bleeding over narrow foreground geometry.
	vec2 offset = 0.25 / uScreenSize;
	float depth = min( min( texture2D( uSceneDepth, uv + vec2( -offset.x, -offset.y ) ).r,
		texture2D( uSceneDepth, uv + vec2( offset.x, -offset.y ) ).r ),
		min( texture2D( uSceneDepth, uv + vec2( -offset.x, offset.y ) ).r,
		texture2D( uSceneDepth, uv + vec2( offset.x, offset.y ) ).r ) );
	float separation = viewDepth( depth ) - viewDepth( gl_FragCoord.z );
	float fade = uDepthHack ? 1.0 : clamp( separation / uSoftDistance, 0.0, 1.0 );
	vec4 particle = texture2D( uParticle, vTexCoord ) * vColor;
	float alpha = particle.a * fade;
	// Additive light contributes emission, without increasing accumulated opacity.
	gl_FragColor = vec4( particle.rgb * alpha, uAdditive ? 0.0 : alpha );
}
