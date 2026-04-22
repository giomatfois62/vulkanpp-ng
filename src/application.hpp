#ifndef ENGINE_HPP
#define ENGINE_HPP

#include "vk_engine.hpp"
#include "vk_bvh.hpp"

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
    void cleanupPipelines();
    void createTestScene();
    void createPlanetScene();
    void createLights();
    void updateLights();

    void updatePlanetScene(float dt);
    void cullInstances(std::vector<vke::InstanceData> &instances, const vke::Frustum &frustum);

    VkPipelineLayout pipelineLayout;
    VkPipeline mtlPipeline;
    VkPipeline pbrPipeline;
    VkPipeline clusteredPbrPipeline;
    VkPipeline lightsPipeline;

    // point lights
    int lightsCount = 30;
    glm::vec3 clusterGridSize = glm::vec3(12, 12, 24);
    bool useClustered = true;
    bool showClusters = false;
    bool showLights = true;

    // planet scene
    uint32_t rocksCount = 30000;
    size_t culledInstances = 0;
    uint32_t rockModelIndex;
    double timeToCullInstances;
    bool doCulling = true;
    bool pauseSimulation = false;

    bool paused = true;
    bool multiDraw = true;
    vke::Octree octree;
};

#endif // ENGINE_HPP
