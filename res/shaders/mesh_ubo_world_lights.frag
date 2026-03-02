#version 450
#extension GL_EXT_nonuniform_qualifier: require
#extension GL_EXT_buffer_reference : require

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inTex;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 viewDir;
layout (location = 4) in vec3 fragPos;
layout (location = 5) flat in uint materialIndex;
layout (location = 6) in mat3 TBN;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 1) uniform sampler2D Sampler2D[];

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

struct Light {
    vec4 position;
    vec4 direction;
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
    uint type;
    uint pad[2];
};

layout(buffer_reference, std430, buffer_reference_align = 4) buffer MaterialArray {
    Material data[];
};

/*
layout(buffer_reference, std430, buffer_reference_align = 4) buffer LightArray {
    Light directional;
    Light point;
    Light spot;
};*/

layout(buffer_reference, std430, buffer_reference_align = 4) buffer LightArray {
    uint count;
    uint pad[3];
    Light data[];
};

layout(push_constant) uniform Constants {
    layout(offset = 16) MaterialArray materials;
    layout(offset = 24) LightArray lights;
} pc;

vec3 diffuseColor(Material material) {
    int hasTex = int(material.diffuseTex > 0);

    return hasTex * texture(Sampler2D[material.diffuseTex], inTex).xyz + (1-hasTex) * material.diffuse.xyz;
}

vec3 specularColor(Material material) {
    int hasTex = int(material.specularTex > 0);

    return hasTex * texture(Sampler2D[material.specularTex], inTex).xyz + (1-hasTex) * material.specular.xyz;
}

vec3 getNormal(Material material) {
    int hasTex = int(material.bumpTex > 0);

    return hasTex * normalize(TBN * (texture(Sampler2D[material.bumpTex], inTex).xyz * 2.0 - 1.0)) + (1-hasTex) * normalize(inNormal);
}

// calculates the color when using a directional light.
vec3 calcDirLight(Light light, vec3 normal, vec3 viewDir, Material material)
{
    vec3 lightDir = normalize(-light.direction.xyz);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    //vec3 halfwayDir = normalize(lightDir + viewDir);
    //float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);

    // combine results
    vec3 ambient = light.ambient.xyz * diffuseColor(material);
    vec3 diffuse = light.diffuse.xyz * diff * diffuseColor(material);
    vec3 specular = light.specular.xyz * spec * specularColor(material);

    return ambient + diffuse + specular;
}

// calculates the color when using a point light.
vec3 calcPointLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir, Material material)
{
    vec3 lightDir = normalize(light.position.xyz - fragPos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    //vec3 halfwayDir = normalize(lightDir + viewDir);
    //float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);

    // attenuation
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    // combine results
    vec3 ambient = light.ambient.xyz * diffuseColor(material);
    vec3 diffuse = light.diffuse.xyz * diff * diffuseColor(material);
    vec3 specular = light.specular.xyz * spec * specularColor(material);

    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return ambient + diffuse + specular;
}

// calculates the color when using a spot light.
vec3 calcSpotLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir, Material material)
{
    vec3 lightDir = normalize(light.position.xyz - fragPos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    //vec3 halfwayDir = normalize(lightDir + viewDir);
    //float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);

    // attenuation
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction.xyz));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    // combine results
    vec3 ambient = light.ambient.xyz * diffuseColor(material);
    vec3 diffuse = light.diffuse.xyz * diff * diffuseColor(material);
    vec3 specular = light.specular.xyz * spec * specularColor(material);

    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;

    return ambient + diffuse + specular;
}

void main()
{
    Material material = pc.materials.data[materialIndex];
    LightArray lights = pc.lights;
    vec3 normal = getNormal(material);

    vec3 color = vec3(0.0);

    for (int i = 0; i < lights.count; ++i) {
        if (lights.data[i].type == 1)
            color += calcDirLight(lights.data[i], normal, viewDir, material);
        if (lights.data[i].type == 0)
            color += calcPointLight(lights.data[i], normal, fragPos, viewDir, material);
        if (lights.data[i].type == 2)
            color += calcSpotLight(lights.data[i], normal, fragPos, viewDir, material);
    }

    outFragColor = vec4(color, 1.0);
}

