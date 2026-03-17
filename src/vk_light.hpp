#ifndef VK_LIGHT_HPP
#define VK_LIGHT_HPP

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "vk_volume.hpp"

namespace vke {

enum class LightType : uint32_t {
    Point = 0,
    Directional = 1,
    Spot = 2
};

struct Light {
    glm::vec4 position;
    glm::vec4 direction;
    glm::vec4 ambient;
    glm::vec4 diffuse;
    glm::vec4 specular;
    float constant = 1.0f;
    float linear = 1.0f;
    float quadratic = 1.0f;
    float cutOff;
    float outerCutOff;
    LightType type = LightType::Point;
    uint32_t pad[2];
};

#define MAX_LIGHTS_PER_CLUSTER 100

struct alignas(16) LightCluster {
    glm::vec4 min;
    glm::vec4 max;
    uint32_t count;
    uint32_t lights[MAX_LIGHTS_PER_CLUSTER];
};

vke::Sphere lightSphere(const vke::Light &light, float minValue = 0.01);
vke::Volume lightVolume(const vke::Light &light, float minValue = 0.01);

std::vector<vke::LightCluster> buildLightClusters(float zNear, float zFar, glm::vec3 gridSize, glm::vec2 screenSize, glm::mat4 inverseProj);
void assignLightsToClusters(std::vector<vke::LightCluster> &clusters, const std::vector<vke::Sphere> &lightVolumes);

} // end namespace vke

#endif // VK_LIGHT_HPP
