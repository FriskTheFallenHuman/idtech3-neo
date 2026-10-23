#version 120
uniform sampler2D uSceneDepth;
uniform mat4 uInverseViewProjection, uViewProjection;
uniform vec2 uScreenSize, uScreenOffset;
uniform vec3 uViewOrigin, uOrigin;
uniform vec4 uColor;
uniform float uRadius, uTime, uPhase, uStrength, uFogAmount;

vec3 worldPosition(vec2 uv, float depth) {
    vec4 point = uInverseViewProjection * vec4(uv*2.0-1.0, depth*2.0-1.0, 1.0);
    return point.xyz/point.w;
}

void main() {
    vec2 uv = (gl_FragCoord.xy-uScreenOffset)/uScreenSize;
    vec3 direction = normalize(worldPosition(uv,0.0)-uViewOrigin);
    vec3 toLight = uOrigin-uViewOrigin;
    float alongRay = dot(toLight,direction);
    float perpendicular2 = max(dot(toLight,toLight)-alongRay*alongRay,0.0);
    float extent = uRadius*10.0;
    if (alongRay <= 0.0 || perpendicular2 >= extent*extent) discard;

    // A hidden flame cannot produce an on-screen corona through opaque walls.
    vec4 lightClip = uViewProjection*vec4(uOrigin,1.0);
    if (lightClip.w <= 0.0) discard;
    vec2 lightUV = lightClip.xy/lightClip.w*.5+.5;
    if (lightUV.x < 0.0 || lightUV.x > 1.0 || lightUV.y < 0.0 || lightUV.y > 1.0) discard;
    float lightDistance = length(toLight);
    float centerDepth = texture2D(uSceneDepth,lightUV).r;
    float centerSceneDistance = length(worldPosition(lightUV,centerDepth)-uViewOrigin);
    float visibility = smoothstep(-1.0,2.0,centerSceneDistance-lightDistance);
    if (visibility <= 0.0) discard;

    // Integrate a smooth light volume, clipped at the visible geometry. Its
    // soft boundary intersects the room naturally instead of overlaying walls.
    float halfPath = sqrt(max(extent*extent-perpendicular2,0.0));
    float sceneDistance = length(worldPosition(uv,texture2D(uSceneDepth,uv).r)-uViewOrigin);
    float first = max(0.0,alongRay-halfPath);
    float last = min(alongRay+halfPath,sceneDistance);
    float path = max(last-first,0.0)/(2.0*extent);
    float radial = perpendicular2/(extent*extent);
    float haze = exp(-4.5*radial)*path*(1.0-smoothstep(.65,1.0,radial));
    float core = pow(1.0+perpendicular2/(uRadius*uRadius*2.5),-2.0);
    core *= smoothstep(-1.0,2.0,sceneDistance-alongRay);
    float pulse = .94+.04*sin(uTime*8.0+uPhase)+.02*sin(uTime*17.0+uPhase*.7);
    vec3 warm = mix(vec3(1.0,.48,.10),vec3(.3,.65,1.0),step(.2,uColor.b-uColor.r))*uColor.rgb;
    float glow = core*.28+haze*(.05+.25*uFogAmount)*uStrength;
    float nearFade = smoothstep(uRadius,uRadius*3.0,lightDistance);
    gl_FragColor = vec4(warm*glow*pulse*visibility*nearFade*uColor.a,0.0);
}
