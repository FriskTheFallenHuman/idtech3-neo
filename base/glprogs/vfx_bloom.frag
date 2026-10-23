#version 120
uniform sampler2D uImage;
uniform vec2 uDirection;
uniform float uThreshold;
varying vec2 vUV;

vec3 bright(vec2 uv) {
    vec3 color = texture2D(uImage,uv).rgb;
    float brightness = max(max(color.r,color.g),color.b);
    return color*max(brightness-uThreshold,0.0)/max(brightness,.0001);
}

void main() {
    // The viewport is inferred from normalized screen coordinates supplied by
    // the fixed fullscreen quad; no world or HUD textures enter this filter.
    vec2 uv = vUV;
    vec3 color = bright(uv)*.227027;
    color += (bright(uv+uDirection*1.384615)+bright(uv-uDirection*1.384615))*.316216;
    color += (bright(uv+uDirection*3.230769)+bright(uv-uDirection*3.230769))*.070270;
    gl_FragColor = vec4(color,0.0);
}
