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
        radius = (-light.linear + sqrt(light.linear * light.linear - 4 * (light.constant-1/minValue) * light.quadratic)) / (2 * light.quadratic);
    }

    return vke::Sphere(glm::vec3(light.position), radius);
}

vke::Volume vke::lightVolume(const Light &light, float minValue)
{
    return vke::Volume(lightSphere(light, minValue));
}

std::vector<vke::LightCluster> vke::buildLightClusters(float zNear, float zFar, glm::vec3 gridSize, glm::mat4 inverseProj)
{
    auto rayIntersectZPlane = [&](const glm::vec3 incident, const float zOffset) {
        return incident * zOffset / incident.z;
    };

    /*
    auto rayIntersectZPlane = [&](glm::vec3 endPoint, float zDistance){
        glm::vec3 startPoint = glm::vec3(0, 0, 0);
        glm::vec3 direction = endPoint - startPoint;
        glm::vec3 normal(0.0, 0.0, 1.0); // plane normal

        float t = (zDistance - dot(normal, startPoint)) / dot(normal, direction);

        return startPoint + t * direction; // the parametric form of the line equation
    };*/

    // mynameismjp.wordpress.com/2009/03/10/reconstructing-position-from-depth/
    auto screenToView = [&](glm::vec4 screen) {
        glm::vec4 clip = glm::vec4(
            screen.x * 2.0 - 1.0,
            (1 - screen.y) * 2.0 - 1.0,
            screen.z,
            screen.w
        );
        glm::vec4 view = inverseProj * clip;

        // Normalize
        // stackoverflow.com/questions/25463735/w-coordinate-in-inverse-projection
        view = view / view.w;

        return view;
    };

    size_t clustersCount = gridSize.x * gridSize.y * gridSize.z;
    std::vector<LightCluster> clusters(clustersCount);

    #pragma omp parallel for collapse(3)
    for (size_t i = 0; i < (size_t)gridSize.x; ++i) {
        for (size_t j = 0; j < (size_t)gridSize.y; ++j) {
            for (size_t k = 0; k < (size_t)gridSize.z; ++k) {
                size_t tileIndex = i + j * gridSize.x + k * gridSize.x * gridSize.y;
                glm::vec2 normPerTileSize(1.0f / float(gridSize.x), 1.0f / float(gridSize.y));

                // Min and max point in screen space
                glm::vec4 minSS(i*normPerTileSize.x, j*normPerTileSize.y, -1.0f, 1.0f); // N.B. negative Z!!!
                glm::vec4 maxSS((i+1)*normPerTileSize.x, (j+1)*normPerTileSize.y, 1.0f, 1.0f); // N.B. positive Z!!!

                // Min and max in view space
                glm::vec3 minVS = screenToView(minSS);
                glm::vec3 maxVS = screenToView(maxSS);

                // Near and far of the cluster in view space
                // This is Equation [2] in the Angel Ortiz article
                float tileNear = -zNear * pow(zFar / zNear, float(k) / gridSize.z);
                float tileFar  = -zNear * pow(zFar / zNear, float(k+1) / gridSize.z);

                // Intersection points
                glm::vec3 minNear = rayIntersectZPlane(minVS, tileNear);
                glm::vec3 minFar = rayIntersectZPlane(minVS, tileFar);
                glm::vec3 maxNear = rayIntersectZPlane(maxVS, tileNear);
                glm::vec3 maxFar  = rayIntersectZPlane(maxVS, tileFar);

                clusters[tileIndex].min = glm::vec4(glm::min(glm::min(minNear, minFar),glm::min(maxNear, maxFar)), 0.0);
                clusters[tileIndex].max = glm::vec4(glm::max(glm::max(minNear, minFar),glm::max(maxNear, maxFar)), 0.0);
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
                if (clusters[i].count >= MAX_LIGHTS_PER_CLUSTER) {
                    continue;
                }
                clusters[i].lights[clusters[i].count] = j;
                clusters[i].count++;
            }
        }
    }
}
