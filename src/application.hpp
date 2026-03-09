#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "vk_engine.hpp"
#include "vk_mesh.hpp"
#include "vk_camera.hpp"
#include "vk_bvh.hpp"
#include "vk_light.hpp"

#include <glm/glm.hpp>

class Application : public vke::Engine {
public:
	Application(int argc, char **argv);
	~Application();

protected:
	void onInit() override;
	void onCleanup() override;
	void onResize(int w, int h) override;
    void draw(VkCommandBuffer cmd) override;
    void drawUI() override;
	void update(float dt) override;
    void processEvent(const SDL_Event &event) override;

	void createPipelines();
    void createPBRPipeline();
    void createOffscreenPipeline();
	void cleanupPipelines();
    void loadAssets();
    void cleanupAssets();

    void loadTestScene();
    void loadPlanetScene();
    void loadLights();
    void updateTestScene(float dt);
    void updatePlanetScene(float dt);
    void updateLights(float dt);
    void drawTestScene(VkCommandBuffer cmd);
    void drawPlanetScene(VkCommandBuffer cmd);
    void cullInstances(vke::Mesh &mesh, const vke::Frustum &frustum);

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkPipeline pbrPipeline;
    VkPipeline lightsPipeline;
    VkPipelineLayout offscreenPipelineLayout;
    VkPipeline offscreenPipeline;

    // test scene
    vke::Model model;
    vke::Model sphere;

    // planet scene
    vke::Model planet;
    vke::Model rock;
    uint32_t rocksCount = 20000;
    size_t culledInstances = 0;
    double timeToCullInstances;
    bool doCulling = true;

    struct {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec4 viewPos;
        glm::vec4 light{ 0.0f, -0.0f, 10.0f, 0.0f };
    } sceneData;

    std::vector<vke::Light> lights;
    std::vector<vke::LightCluster> lightClusters;
    double timeToBuildClusters;
    double timeToAssignLights;

    vke::UBOs sceneDataBuffers;
    vke::SSBOs lightDataBuffers;
    vke::UBOs materialBuffers;
    vke::UBOs pbrMaterialBuffers;

    vke::Camera camera;
    float fov = 45.f;
    uint32_t instances = 1;
    uint32_t lightsCount = 1;
    float objScale = 1.0f;
    glm::vec3 objPosition = {};
    glm::vec3 objRotation = {};
    bool paused = true;
    std::vector<vke::Material> backupMaterials;
    vke::Octree octree;
};

#endif // ENGINE_HPP
