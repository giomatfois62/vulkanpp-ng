#include "vk_light.hpp"

#include <cmath>

vke::Sphere vke::lightSphere(const Light &light, float minValue)
{
    if (light.type == LightType::Directional) {
        return vke::Sphere(glm::vec3(0), FLT_MAX);
    }

    // solve kq*d^2 + kl*d + kc - 1/minValue = 0
    float radius;
    float eps = FLT_EPSILON;

    if (light.linear <= eps && light.quadratic <= eps) {
        radius = FLT_MAX;
    } else if (light.quadratic <= eps) {
        radius = (-light.constant+1/minValue)/light.linear;
    } else {
        radius = (-light.constant + sqrt(light.linear * light.linear - 4 * (light.constant-1/minValue) * light.quadratic)) / (2 * light.quadratic);
    }

    return vke::Sphere(glm::vec3(light.position), radius);
}

vke::Volume vke::lightVolume(const Light &light, float minValue)
{
    return vke::Volume(lightSphere(light, minValue));
}

// https://github.com/DaveH355/clustered-shading
std::vector<vke::LightCluster> vke::buildLightClusters(float zNear, float zFar, glm::vec3 gridSize, glm::vec2 screenSize, glm::mat4 inverseProj)
{
    // Returns the intersection point of an infinite line and a
    // plane perpendicular to the Z-axis
    auto lineIntersectionWithZPlane = [&](glm::vec3 startPoint, glm::vec3 endPoint, float zDistance){
        glm::vec3 direction = endPoint - startPoint;
        glm::vec3 normal(0.0, 0.0, -1.0); // plane normal

        float t = (zDistance - dot(normal, startPoint)) / dot(normal, direction);

        return startPoint + t * direction; // the parametric form of the line equation
    };

    auto screenToView = [&](glm::vec2 screenCoords) {
        // normalize screenCoord to [-1, 1] and
        // set the NDC depth of the coordinate to be on the near plane. This is -1 by
        // default in OpenGL
        glm::vec4 ndc = glm::vec4(
            screenCoords.x / screenSize.x * 2.0 - 1.0,
            screenCoords.y / screenSize.y * 2.0 - 1.0,
            -1.0,
            1.0
        );

        glm::vec4 viewCoords = inverseProj * ndc;
        viewCoords /= viewCoords.w;

        return viewCoords;
    };

    size_t clustersCount = gridSize.x * gridSize.y * gridSize.z;
    std::vector<LightCluster> clusters(clustersCount);

    #pragma omp parallel for collapse(3)
    for (size_t i = 0; i < (size_t)gridSize.x; ++i) {
        for (size_t j = 0; j < (size_t)gridSize.y; ++j) {
            for (size_t k = 0; k < (size_t)gridSize.z; ++k) {
                size_t tileIndex = i + j * gridSize.x + k * gridSize.x * gridSize.y;
                glm::vec2 tileSize(screenSize.x / gridSize.x, screenSize.y / gridSize.y);

                // tile in screen-space
                glm::vec2 minTile_screenspace = glm::vec2(i,j) * tileSize;
                glm::vec2 maxTile_screenspace = glm::vec2(i+1,j+1) * tileSize;

                // convert tile to view space sitting on the near plane
                glm::vec3 minTile = screenToView(minTile_screenspace);
                glm::vec3 maxTile = screenToView(maxTile_screenspace);

                float planeNear = zNear * pow(zFar / zNear, k / float(gridSize.z));
                float planeFar  = zNear * pow(zFar / zNear, (k + 1) / float(gridSize.z));

                // the line goes from the eye position in view space (0, 0, 0)
                // through the min/max points of a tile to intersect with a given cluster's near-far planes
                glm::vec3 minPointNear = lineIntersectionWithZPlane(glm::vec3(0, 0, 0), minTile, planeNear);
                glm::vec3 minPointFar  = lineIntersectionWithZPlane(glm::vec3(0, 0, 0), minTile, planeFar);
                glm::vec3 maxPointNear = lineIntersectionWithZPlane(glm::vec3(0, 0, 0), maxTile, planeNear);
                glm::vec3 maxPointFar  = lineIntersectionWithZPlane(glm::vec3(0, 0, 0), maxTile, planeFar);

                clusters[tileIndex].min = glm::vec4(min(minPointNear, minPointFar), 0.0);
                clusters[tileIndex].max = glm::vec4(max(maxPointNear, maxPointFar), 0.0);
                clusters[tileIndex].count = 0;
            }
        }
    }

    return clusters;
}

void vke::assignLightsToClusters(std::vector<LightCluster> &clusters, const std::vector<Sphere> &lightVolumes)
{
    #pragma omp parallel for
    for (size_t i = 0; i < clusters.size(); ++i) {
        for (size_t j = 0; j < lightVolumes.size(); ++j) {
            if (Volume(clusters[i].min, clusters[i].max).intersect(lightVolumes[j])) {
                clusters[i].lights[clusters[i].count] = j;
                clusters[i].count++;
            }
        }
    }
}
