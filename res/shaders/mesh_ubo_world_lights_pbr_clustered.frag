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

const float PI = 3.14159265359;

struct PBRMaterial {
    vec4 baseColor;
    float metallic;
    float roughness;
    uint baseColorTex;
    uint metallicRoughnessTex;
    uint normalTex;
    uint occlusionTex;
    uint pad[2];
};

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

struct LightCluster {
    vec4 min;
    vec4 max;
    uint count;
    float pad[3];
    uint lights[128];
};

layout(buffer_reference, std430, buffer_reference_align = 4) buffer MaterialArray {
    PBRMaterial data[];
};

layout(buffer_reference, std430, buffer_reference_align = 4) buffer LightClusterArray {
    LightCluster data[];
};

layout(buffer_reference, std430, buffer_reference_align = 4) buffer LightArray {
    uint count;
    uint pad[3];
    Light data[];
};

layout(buffer_reference, std430, buffer_reference_align = 4) buffer LightClusterInfo {
    vec2 screenSize;
    float pad[2];
    vec3 clusterGridSize;
    int showCluster;
    float zNear;
    float zFar;
    float scale;
    float bias;
};

layout(push_constant) uniform Constants {
    layout(offset = 16) MaterialArray materials;
    layout(offset = 24) LightArray lights;
    layout(offset = 32) LightClusterArray lightClusters;
    layout(offset = 40) LightClusterInfo lightClusterInfo;
} pc;

float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float linearDepth(float depthSample, float zNear, float zFar){
    // https://stackoverflow.com/questions/51108596/linearize-depth
    //return zNear * zFar / (zFar + depthSample * (zNear - zFar));

    //float depthRange = 2.0 * depthSample - 1.0;
    float depthRange = depthSample;
    // Near... Far... wherever you are...
    float linear = 2.0 * zNear * zFar / (zFar + zNear - depthRange * (zFar - zNear));
    return linear;
}

void main()
{
    PBRMaterial material = pc.materials.data[materialIndex];

    // pow(albedo, vec3(2.2)) ?
    vec3 albedo = (material.baseColorTex > 0) ? texture(Sampler2D[material.baseColorTex], inTex).xyz  * material.baseColor.xyz : material.baseColor.xyz;
    float metallic = (material.metallicRoughnessTex > 0) ? texture(Sampler2D[material.metallicRoughnessTex], inTex).b * material.metallic : material.metallic;
    float roughness = (material.metallicRoughnessTex > 0) ? texture(Sampler2D[material.metallicRoughnessTex], inTex).g * material.roughness : material.roughness;
    float ao = (material.occlusionTex > 0) ? texture(Sampler2D[material.occlusionTex], inTex).r : 1.0;

    vec3 N = (material.normalTex > 0) ? normalize(TBN * (texture(Sampler2D[material.normalTex], inTex).xyz * 2.0 - 1.0)) : normalize(inNormal);
    vec3 V = normalize(viewDir);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);

    // reflectance equation
    vec3 Lo = vec3(0.0);

    // light cluster index
    uvec3 gridSize = uvec3(pc.lightClusterInfo.clusterGridSize.xyz);
    vec2 screenDimensions = pc.lightClusterInfo.screenSize.xy;
    float zNear = pc.lightClusterInfo.zNear;
    float zFar = pc.lightClusterInfo.zFar;
    float scale = pc.lightClusterInfo.scale;
    float bias = pc.lightClusterInfo.bias;

    vec2 tileSize = screenDimensions / gridSize.xy;
    uint zTile = uint(max(log2(linearDepth(gl_FragCoord.z, zNear, zFar)) * scale + bias, 0.0));
    uvec3 tile = uvec3(gl_FragCoord.xy / tileSize, zTile);
    //tile.y = uint((screenDimensions.y-gl_FragCoord.y)/tileSize.y);
    uint tileIndex = tile.x + (tile.y * gridSize.x) + (tile.z * gridSize.x * gridSize.y);
    uint lightCount = pc.lightClusters.data[tileIndex].count;

    if (pc.lightClusterInfo.showCluster > 0) {
        outFragColor = vec4(vec3((tile.z)/float(gridSize.z-1)),1);
        return;
    }
    //outFragColor = vec4(vec3((tile.z)/float(gridSize.z-1)),1);
    //outFragColor = vec4(vec3((tile.y)/float(gridSize.y-1)),1);
    //outFragColor = vec4(vec3(gl_FragCoord.z),1);
    //outFragColor = vec4(vec3(linearDepth(gl_FragCoord.z, zNear, zFar)),1);
    //return;

    if (lightCount >= 100) {
        //getting close to limit. Output red color and dip
        outFragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
        return;
    }

    for(int i = 0; i < pc.lightClusters.data[tileIndex].count; ++i) {
        uint lightIndex = pc.lightClusters.data[tileIndex].lights[i];
    //for(int i = 0; i < pc.lights.count; ++i) {
    //    uint lightIndex = i;
        Light light = pc.lights.data[lightIndex];
        //Light light = pc.lights.data[i];

        vec3 L;
        vec3 radiance;

        if (light.type == 1) {
            // directional light
            L = normalize(-light.direction.xyz);
            radiance = light.diffuse.rgb;
        } else {
            // point/spot light
            vec3 toLight = light.position.xyz - fragPos;
            L = normalize(toLight);

            float distance = length(toLight);
            //float attenuation = 1.0 / max(distance * distance, 0.001);
            //float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
            float attenuation = light.intensity / (distance * distance + 1);
            attenuation *= pow(clamp(1.0 - pow(distance / light.radius, 4.0), 0.0, 1.0), 2.0);

            radiance = light.diffuse.rgb * attenuation;

            if (light.type == 2) {
                // spotlight intensity
                float theta = dot(L, normalize(-light.direction.xyz));
                float epsilon = light.cutOff - light.outerCutOff;
                float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
                // TODO: pow(intensity, 2) ?
                radiance *= intensity;
            }
        }

        vec3 H = normalize(V + L);

        // Cook-Torrance BRDF
        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        vec3 specular = numerator / denominator;

        // kS is equal to Fresnel
        vec3 kS = F;
        // for energy conservation, the diffuse and specular light can't
        // be above 1.0 (unless the surface emits light); to preserve this
        // relationship the diffuse component (kD) should equal 1.0 - kS.
        vec3 kD = vec3(1.0) - kS;
        // multiply kD by the inverse metalness such that only non-metals
        // have diffuse lighting, or a linear blend if partly metal (pure metals
        // have no diffuse light).
        kD *= 1.0 - metallic;

        // scale light by NdotL
        float NdotL = max(dot(N, L), 0.0);

        // add to outgoing radiance Lo
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
    }

    // ambient lighting (note that the next IBL tutorial will replace
    // this ambient lighting with environment lighting).
    vec3 ambient = vec3(0.00001) * albedo * ao;

    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    //color = pow(color, vec3(1.0/2.2));

    outFragColor = vec4(color, 1.0);
    //outFragColor = vec4(1,0,0,1);
}
