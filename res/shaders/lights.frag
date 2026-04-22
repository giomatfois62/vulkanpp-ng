#version 450
#extension GL_EXT_nonuniform_qualifier: require
#extension GL_EXT_buffer_reference : require

layout (location = 0) flat in uint index;

layout (location = 0) out vec4 outFragColor;

struct Light {
    vec4 position;
    vec4 direction;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float intensity;
    float radius;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
    uint type;
};

layout(buffer_reference, std430, buffer_reference_align = 4) buffer LightArray {
    uint count;
    uint pad[3];
    Light data[];
};

layout(push_constant) uniform Constants {
    layout(offset = 24) LightArray lights;
} pc;

void main()
{
    vec3 color = pc.lights.data[index].diffuse.xyz;
    outFragColor = vec4(color, 1.0);
}

