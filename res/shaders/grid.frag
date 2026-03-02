#version 450 core

// http://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/

layout (location = 0) in vec3 nearPoint;
layout (location = 1) in vec3 farPoint;
layout (location = 2) in mat4 fragView;
layout (location = 6) in mat4 fragProj;
layout (location = 10) in float near;
layout (location = 11) in float far;

layout (location = 0) out vec4 fragColor;

vec4 grid(vec3 fragPos3D, float scale) {
    vec2 coord = fragPos3D.xy * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    vec4 color = vec4(0.2, 0.2, 0.2, 1.0 - min(line, 1.0));

    float minimumy = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);

    // x axis
    if(fragPos3D.x > -1* minimumx && fragPos3D.x < 1 * minimumx)
        color.z = 1.0;

    // y axis
    if(fragPos3D.y > -1 * minimumy && fragPos3D.y < 1 * minimumy)
        color.x = 1.0;

    return color;
}

float computeDepth(vec3 pos) {
    vec4 position = fragProj * fragView * vec4(pos.xyz, 1.0);

    return (position.z / position.w);
}

float computeLinearDepth(vec3 pos) {
    vec4 position = fragProj * fragView * vec4(pos.xyz, 1.0);
    float depth = (position.z / position.w) * 2.0 - 1.0; // put back between -1 and 1
    float linearDepth = (2.0 * near * far) / (far + near - depth * (far - near)); // get linear value between near and far

    return linearDepth / far; // normalize
}

void main()
{
    float t = -nearPoint.z / (farPoint.z - nearPoint.z);
    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);

    gl_FragDepth = computeDepth(fragPos3D);

    float linearDepth = computeLinearDepth(fragPos3D);
    float fading = max(0, (0.5 - linearDepth));

    fragColor = (grid(fragPos3D, 1) + grid(fragPos3D, 1))* float(t > 0); // adding multiple resolution for the grid
    fragColor.a *= fading;
}
