#version 450
#extension GL_EXT_buffer_reference : require

// http://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/

layout (location = 0) in vec3 position;

layout (location = 0) out vec3 nearPoint;
layout (location = 1) out vec3 farPoint;
layout (location = 2) out mat4 fragView;
layout (location = 6) out mat4 fragProj;
layout (location = 10) out float near;
layout (location = 11) out float far;


layout(buffer_reference, std430, buffer_reference_align = 16) buffer ShaderData {
    mat4 projection;
    mat4 view;
    vec4 viewPos;
    mat4 viewInv;
    mat4 projInv;
    float znear;
    float zfar;
};

// Push constant holds the 64-bit addresses
layout(push_constant) uniform Constants {
    ShaderData data;
} pc;

vec3 unprojectPoint(float x, float y, float z, mat4 view, mat4 projection) {
    vec4 unprojectedPoint =  pc.data.viewInv * pc.data.projInv * vec4(x, y, z, 1.0);

    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main()
{
    nearPoint = unprojectPoint(position.x, position.y, 0.0, pc.data.view, pc.data.projection).xyz; // unprojecting on the near plane
    farPoint = unprojectPoint(position.x, position.y, 1.0, pc.data.view, pc.data.projection).xyz; // unprojecting on the far plane

    fragView = pc.data.view;
    fragProj = pc.data.projection;
    near = pc.data.znear;
    far = pc.data.zfar;

    gl_Position = vec4(position, 1.0); // using directly the clipped coordinates
}
