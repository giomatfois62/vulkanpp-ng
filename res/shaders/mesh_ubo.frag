#version 450
#extension GL_EXT_nonuniform_qualifier: require
#extension GL_EXT_buffer_reference : require

//shader input
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inTex;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 lightVec;
layout (location = 4) in vec3 viewVec;
layout (location = 5) flat in int instanceIndex;
layout (location = 6) flat in uint materialIndex;

//output write
layout (location = 0) out vec4 outFragColor;

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

struct Material {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float shininess;
    uint ambientTex;
    uint diffuseTex;
    uint specularTex;
    uint bumpTex;
    uint pad[3];
};

layout(buffer_reference, std430, buffer_reference_align = 4) buffer MaterialArray {
    Material data[];
};

layout (set = 0, binding = 1) uniform sampler2D Sampler2D[];

// Push constant holds the 64-bit addresses
layout(push_constant) uniform Constants {
    ShaderData shaderData;
    MeshDataArray meshData;
    MaterialArray materials;
} pc;

vec3 diffuseColor(Material mat) {
    int hasTex = int(mat.diffuseTex > 0);
    return vec3(hasTex) * texture(Sampler2D[mat.diffuseTex], inTex).xyz + vec3(1 - hasTex) * mat.diffuse.xyz;
}

vec3 specularColor(Material mat) {
    int hasTex = int(mat.specularTex > 0);
    return vec3(hasTex) * texture(Sampler2D[mat.specularTex], inTex).xyz + vec3(1 - hasTex) * mat.specular.xyz;
}

vec3 normal(Material mat) {
    int hasTex = int(mat.bumpTex > 0);
    return vec3(hasTex) * normalize(texture(Sampler2D[mat.bumpTex], inTex).xyz * 2.0 - 1.0) + vec3(1 - hasTex) * normalize(inNormal);
}

void main()
{
    Material mat = pc.materials.data[materialIndex];
    mat4 view = pc.shaderData.view;

    vec3 N = normal(mat);
    vec3 L = normalize(lightVec);
    vec3 V = normalize(viewVec);
    vec3 R = reflect(-L, N);

    float diffuse = max(dot(N, L), 0.0025);
    float specular = pow(max(dot(R, V), 0.0), mat.shininess) * 0.75;

    vec3 color = diffuseColor(mat);
    vec3 specColor = specularColor(mat);

    outFragColor = vec4(diffuse * color.rgb + specular * specColor.rgb, 1.0);
}

