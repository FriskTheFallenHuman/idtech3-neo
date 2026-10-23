#version 120
// Horizon search ported from E:/projects/Heretic/code/hbao.cpp.
uniform sampler2D uSceneDepth;
uniform vec2 uSceneSize;
uniform vec4 uProjection;
uniform vec3 uSettings;

float depthAt( vec2 pixel ) {
	return texture2D( uSceneDepth, ( clamp( pixel, vec2( 0.0 ), uSceneSize - 1.0 ) + 0.5 ) / uSceneSize ).r;
}
vec3 positionAt( vec2 pixel ) {
	float depth = depthAt( pixel );
	// OpenGL window depth [0,1] -> eye-space distance, using this view's projection.
	float z = uProjection.w / ( depth * 2.0 - 1.0 + uProjection.z );
	vec2 ndc = ( pixel + 0.5 ) / uSceneSize * 2.0 - 1.0;
	return vec3( ndc.x * z / uProjection.x, ndc.y * z / uProjection.y, z );
}
void main() {
	vec2 id = floor( gl_FragCoord.xy );
	vec2 pixel = min( id * 2.0 + 1.0, uSceneSize - 1.0 );
	if ( depthAt( pixel ) >= 0.999999 ) {
		gl_FragColor = vec4( 1.0, 65000.0, 0.0, 1.0 );
		return;
	}
	vec3 center = positionAt( pixel );
	vec3 dx1 = positionAt( pixel + vec2( 1, 0 ) ) - center;
	vec3 dx2 = center - positionAt( pixel - vec2( 1, 0 ) );
	vec3 dy1 = positionAt( pixel + vec2( 0, 1 ) ) - center;
	vec3 dy2 = center - positionAt( pixel - vec2( 0, 1 ) );
	vec3 dx = dot( dx1, dx1 ) < dot( dx2, dx2 ) ? dx1 : dx2;
	vec3 dy = dot( dy1, dy1 ) < dot( dy2, dy2 ) ? dy1 : dy2;
	vec3 normal = cross( dx, dy );
	normal *= inversesqrt( max( dot( normal, normal ), 1e-12 ) );
	if ( dot( normal, -center ) < 0.0 ) {
		normal = -normal;
	}
	float radius = uSettings.x;
	float pixelRadius = clamp( radius * uProjection.y * uSceneSize.y / ( 2.0 * center.z ), 1.0, 128.0 );
	float rotation = fract( dot( mod( id, 4.0 ), vec2( 0.06711056, 0.00583715 ) ) * 52.9829189 ) * 6.2831853;
	float occlusion = 0.0;
	for ( int direction = 0; direction < 8; ++direction ) {
		float angle = float( direction ) * 0.78539816 + rotation;
		vec2 axis = vec2( cos( angle ), sin( angle ) );
		float horizon = uSettings.z;
		for ( int stepIndex = 1; stepIndex <= 4; ++stepIndex ) {
			float fraction = float( stepIndex ) / 4.0;
			vec2 q = pixel + floor( axis * max( 1.0, pixelRadius * fraction * fraction ) + 0.5 );
			if ( any( lessThan( q, vec2( 0.0 ) ) ) || any( greaterThanEqual( q, uSceneSize ) ) || depthAt( q ) >= 0.999999 ) {
				continue;
			}
			vec3 delta = positionAt( q ) - center;
			float distance2 = dot( delta, delta );
			if ( distance2 >= radius * radius || distance2 < 1e-8 ) {
				continue;
			}
			float elevation = dot( normal, delta ) * inversesqrt( distance2 );
			float falloff = clamp( 1.0 - distance2 / ( radius * radius ), 0.0, 1.0 );
			occlusion += max( elevation - horizon, 0.0 ) * falloff;
			horizon = max( horizon, elevation );
		}
	}
	float visibility = clamp( 1.0 - occlusion / ( 8.0 * ( 1.0 - uSettings.z ) ), 0.0, 1.0 );
	gl_FragColor = vec4( pow( visibility, 2.5 * uSettings.y ), min( center.z, 65000.0 ), 0.0, 1.0 );
}
