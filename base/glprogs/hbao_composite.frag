#version 120
// Heretic's depth-aware nine-tap upsample. RGB multiplication preserves alpha.
uniform sampler2D uSceneDepth;
uniform sampler2D uAmbient;
uniform vec2 uSceneSize, uScreenOffset;
uniform vec4 uProjection;
uniform vec3 uSettings;

void main() {
	vec2 pixel = floor( gl_FragCoord.xy - uScreenOffset );
	float depth = texture2D( uSceneDepth, ( pixel + 0.5 ) / uSceneSize ).r;
	if ( depth >= 0.999999 ) {
		gl_FragColor = vec4( 1.0 );
		return;
	}
	float z = min( uProjection.w / ( depth * 2.0 - 1.0 + uProjection.z ), 65000.0 );
	vec2 size = ceil( uSceneSize * 0.5 );
	vec2 base = floor( pixel * 0.5 );
	float sum = 0.0, weights = 0.0;
	for ( int y = -1; y <= 1; ++y ) {
		for ( int x = -1; x <= 1; ++x ) {
			vec2 q = clamp( base + vec2( x, y ), vec2( 0.0 ), size - 1.0 );
			vec2 sampleValue = texture2D( uAmbient, ( q + 0.5 ) / size ).rg;
			vec2 delta = q * 2.0 + 1.5 - ( pixel + 0.5 );
			float weight = exp2( -dot( delta, delta ) * 0.25 - abs( sampleValue.y - z ) / max( uSettings.x * 0.05, 0.5 ) );
			sum += sampleValue.x * weight;
			weights += weight;
		}
	}
	float visibility = weights > 1e-5 ? sum / weights : 1.0;
	gl_FragColor = vec4( visibility, visibility, visibility, 1.0 );
}
