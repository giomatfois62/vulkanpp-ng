#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) in vec3 vPosition;

layout (location = 0) out uint index;

layout(buffer_reference, std430, buffer_reference_align = 16) buffer ShaderData {
    mat4 projection;
    mat4 view;
    vec4 viewPos;
};

struct MeshData {
    uint material;
    uint pad[3];
    mat4 transform;
};

layout(buffer_reference, std430, buffer_reference_align = 16) buffer MeshDataArray {
    MeshData data[];
};

// Push constant holds the 64-bit addresses
layout(push_constant) uniform Constants {
    ShaderData shaderData;
    MeshDataArray meshData;
} pc;

void main()
{
    mat4 proj = pc.shaderData.projection;
    mat4 view = pc.shaderData.view;
    mat4 model = pc.meshData.data[gl_InstanceIndex].transform;

    index = gl_InstanceIndex;
    gl_Position = proj * view * model * vec4(vPosition, 1.0f);
}
