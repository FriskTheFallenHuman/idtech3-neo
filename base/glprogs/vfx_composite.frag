#version 120

uniform sampler2D uParticles;
uniform sampler2D uSceneDepth;
uniform sampler2D uBloom;
uniform float uBloomScale;
uniform vec2 uScreenSize;
uniform vec2 uScreenOffset;
uniform vec2 uParticleSize;
uniform vec2 uSceneSize;
uniform vec2 uProjectionDepth;

float viewDepth( float depth ) {
	return uProjectionDepth.y / ( depth * 2.0 - 1.0 + uProjectionDepth.x );
}

float guideDepth( vec2 uv ) {
	vec2 offset = 0.25 / uParticleSize;
	float depth = min( min( texture2D( uSceneDepth, uv + vec2( -offset.x, -offset.y ) ).r,
		texture2D( uSceneDepth, uv + vec2( offset.x, -offset.y ) ).r ),
		min( texture2D( uSceneDepth, uv + vec2( -offset.x, offset.y ) ).r,
		texture2D( uSceneDepth, uv + vec2( offset.x, offset.y ) ).r ) );
	return viewDepth( depth );
}

void main() {
	vec2 uv = ( gl_FragCoord.xy - uScreenOffset ) / uScreenSize;
	float targetDepth = viewDepth( texture2D( uSceneDepth, uv ).r );
	vec2 pixel = uv * uParticleSize - 0.5;
	vec2 base = floor( pixel );
	vec2 fraction = fract( pixel );
	vec4 result = vec4( 0.0 );
	float total = 0.0;
	for ( int y = 0; y < 2; ++y ) {
		for ( int x = 0; x < 2; ++x ) {
			vec2 coordinate = ( base + vec2( float( x ), float( y ) ) + 0.5 ) / uParticleSize;
			float depth = guideDepth( coordinate );
			float weight = ( x == 0 ? 1.0 - fraction.x : fraction.x ) *
				( y == 0 ? 1.0 - fraction.y : fraction.y );
			weight *= exp( -abs( targetDepth - depth ) / max( 1.0, targetDepth * 0.015 ) );
			result += texture2D( uParticles, coordinate ) * weight;
			total += weight;
		}
	}
	// No compatible neighbor means an opaque edge; suppress the particle there.
	vec4 color = total > 0.00001 ? result / total : vec4( 0.0 );
	if ( uBloomScale > 0.0 ) color.rgb += texture2D(uBloom,uv).rgb*uBloomScale;
	gl_FragColor = color;
}
