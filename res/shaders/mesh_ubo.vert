#version 450
#extension GL_EXT_buffer_reference : require

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec3 vNormal;
layout (location = 2) in vec3 vTangent;
layout (location = 3) in vec3 vBitangent;
layout (location = 4) in vec2 vTex;
layout (location = 5) in vec3 vColor;
layout (location = 6) in uint vMaterial;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outTex;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec3 lightVec;
layout (location = 4) out vec3 viewVec;
layout (location = 5) out int instanceIndex;
layout (location = 6) out uint materialIndex;

layout(buffer_reference, std430, buffer_reference_align = 16) buffer ShaderData {
    mat4 projection;
    mat4 view;
    vec4 viewPos;
    vec4 light;
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
    MeshDataArray dummy;
} pc;

void main()
{
    mat4 proj = pc.shaderData.projection;
    mat4 view = pc.shaderData.view;
    mat4 model = pc.meshData.data[gl_InstanceIndex].transform;
    vec3 light = vec3(pc.shaderData.light);
    mat4 viewModel = view * model;
    vec3 fragPos =  vec3(viewModel * vec4(vPosition, 1.0f));

    gl_Position = proj * view * model * vec4(vPosition, 1.0f);
    outColor = vColor;
    outTex = vTex;
    outNormal = mat3(viewModel) * vNormal;
    lightVec = light - fragPos;
    viewVec = -fragPos;
    instanceIndex = gl_InstanceIndex;
    materialIndex = pc.meshData.data[gl_InstanceIndex].material;
}
