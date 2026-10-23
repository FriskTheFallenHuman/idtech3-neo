#version 120

uniform sampler2D uDecal;
uniform sampler2D uSceneDepth;
uniform mat4 uInverseViewProjection;
uniform vec2 uScreenSize;
uniform vec2 uScreenOffset;
uniform vec3 uOrigin;
uniform vec3 uNormal;
uniform vec3 uTangent;
uniform vec3 uBitangent;
uniform float uRadius;
uniform float uThickness;
varying vec4 vColor;

void main() {
	vec2 uv = ( gl_FragCoord.xy - uScreenOffset ) / uScreenSize;
	float depth = texture2D( uSceneDepth, uv ).r;
	vec4 projected = uInverseViewProjection * vec4( uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0 );
	vec3 position = projected.xyz / projected.w;
	// Derivatives must be evaluated before divergent discards at projector edges.
	vec3 dx = dFdx( position ), dy = dFdy( position );
	vec3 crossNormal = cross( dx, dy );
	float normalLength = length( crossNormal );
	vec3 normal = crossNormal / max( normalLength, 0.00001 );
	vec3 relative = position - uOrigin;
	vec2 decalUV = vec2( dot( relative, uTangent ), dot( relative, uBitangent ) ) / ( 2.0 * uRadius ) + 0.5;
	float planeDistance = abs( dot( relative, uNormal ) );
	if ( depth >= 0.999999 || normalLength < 0.00001 || abs( dot( normal, uNormal ) ) < 0.55 ||
		planeDistance > uThickness || any( lessThan( decalUV, vec2( 0.0 ) ) ) ||
		any( greaterThan( decalUV, vec2( 1.0 ) ) ) ) {
		discard;
	}
	vec2 edge = min( decalUV, 1.0 - decalUV );
	float edgeFade = smoothstep( 0.0, 0.015, min( edge.x, edge.y ) );
	vec4 decal = texture2D( uDecal, decalUV ) * vColor;
	decal.a *= edgeFade * ( 1.0 - smoothstep( uThickness * 0.65, uThickness, planeDistance ) );
	gl_FragColor = decal;
}
