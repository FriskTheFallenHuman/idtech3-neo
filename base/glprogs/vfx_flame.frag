#version 120

uniform sampler2D uSceneDepth;
uniform mat4 uInverseViewProjection;
uniform vec2 uScreenSize;
uniform vec2 uSceneSize;
uniform vec3 uViewOrigin;
uniform vec3 uOrigin;
uniform float uRadius;
uniform float uTime;
uniform float uPhase;
uniform vec4 uColor;

vec3 worldPosition( vec2 uv, float depth ) {
	vec4 point = uInverseViewProjection * vec4( uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0 );
	return point.xyz / point.w;
}

float hash( vec3 point ) {
	return fract( sin( dot( point, vec3( 127.1, 311.7, 74.7 ) ) ) * 43758.5453 );
}

float noise( vec3 point ) {
	vec3 cell = floor( point );
	vec3 f = fract( point );
	f = f * f * ( 3.0 - 2.0 * f );
	return mix( mix( mix( hash( cell ), hash( cell + vec3( 1, 0, 0 ) ), f.x ),
		mix( hash( cell + vec3( 0, 1, 0 ) ), hash( cell + vec3( 1, 1, 0 ) ), f.x ), f.y ),
		mix( mix( hash( cell + vec3( 0, 0, 1 ) ), hash( cell + vec3( 1, 0, 1 ) ), f.x ),
		mix( hash( cell + vec3( 0, 1, 1 ) ), hash( cell + vec3( 1, 1, 1 ) ), f.x ), f.y ), f.z );
}

void main() {
	vec2 uv = gl_FragCoord.xy / uScreenSize;
	vec3 nearPoint = worldPosition( uv, 0.0 );
	vec3 direction = normalize( nearPoint - uViewOrigin );
	vec3 safeDirection = mix( vec3( -1.0 ), vec3( 1.0 ), step( vec3( 0.0 ), direction ) ) * max( abs( direction ), vec3( 0.00001 ) );
	vec3 minimum = uOrigin + vec3( -2.0, -2.0, 0.0 ) * uRadius;
	vec3 maximum = uOrigin + vec3( 2.0, 2.0, 8.0 ) * uRadius;
	vec3 first = ( minimum - uViewOrigin ) / safeDirection;
	vec3 last = ( maximum - uViewOrigin ) / safeDirection;
	vec3 starts = min( first, last ), ends = max( first, last );
	float begin = max( max( starts.x, starts.y ), max( starts.z, length( nearPoint - uViewOrigin ) ) );
	float end = min( min( ends.x, ends.y ), ends.z );
	vec2 offset = 0.25 / uScreenSize;
	float sceneDepth = min( min( texture2D( uSceneDepth, uv + vec2( -offset.x, -offset.y ) ).r,
		texture2D( uSceneDepth, uv + vec2( offset.x, -offset.y ) ).r ),
		min( texture2D( uSceneDepth, uv + vec2( -offset.x, offset.y ) ).r,
		texture2D( uSceneDepth, uv + vec2( offset.x, offset.y ) ).r ) );
	end = min( end, length( worldPosition( uv, sceneDepth ) - uViewOrigin ) );
	if ( end <= begin ) {
		discard;
	}
	float stepLength = ( end - begin ) / 16.0;
	float transmission = 1.0;
	vec3 emission = vec3( 0.0 );
	for ( int sampleIndex = 0; sampleIndex < 16; ++sampleIndex ) {
		float distance = begin + ( float( sampleIndex ) + 0.5 ) * stepLength;
		vec3 p = ( uViewOrigin + direction * distance - uOrigin ) / uRadius;
		float height = p.z / 8.0;
		// Advect coherent world-space turbulence upward; each emitter has its own phase.
		vec3 flow = p * vec3( 1.3, 1.3, 0.85 ) + vec3( uPhase, uPhase * 0.7, -uTime * 3.8 );
		float coarse = noise( flow );
		float fine = noise( flow * 2.4 + vec3( 0.0, 0.0, -uTime * 1.7 ) );
		vec2 bend = vec2( sin( p.z * 0.7 - uTime * 2.3 + uPhase ), cos( p.z * 0.9 - uTime * 1.9 + uPhase ) );
		bend *= height * 0.38;
		float width = max( 0.05, 0.82 * ( 1.0 - height ) );
		float radial = length( p.xy - bend ) / width;
		float density = max( 0.0, 1.35 - radial + ( coarse - 0.5 ) * 1.0 + ( fine - 0.5 ) * 0.4 );
		density *= smoothstep( 0.0, 0.05, height ) * ( 1.0 - smoothstep( 0.55, 0.98, height + ( coarse - 0.5 ) * 0.2 ) );
		float heat = clamp( 1.0 - radial * 0.45 - height * 0.55 + coarse * 0.15, 0.0, 1.0 );
		vec3 color = mix( vec3( 1.0, 0.09, 0.005 ), vec3( 1.0, 0.7, 0.08 ), smoothstep( 0.15, 0.75, heat ) );
		color = mix( color, vec3( 1.0, 0.96, 0.7 ), smoothstep( 0.75, 1.0, heat ) );
		float absorption = 1.0 - exp( -density * stepLength / uRadius * 1.4 * uColor.a );
		emission += transmission * absorption * color * uColor.rgb * ( 1.5 + heat * 1.1 );
		transmission *= 1.0 - absorption;
	}
	gl_FragColor = vec4( emission, 1.0 - transmission );
}
