#include "application.hpp"
#include "vk_pipeline.hpp"
#include "vk_utils.hpp"
#include "vk_geometry.hpp"
#include "vk_ui.hpp"

#include "imgui.h"
#include <random>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace std;
using namespace vke;

Application::Application(int argc, char **argv) :
	Engine(argc, argv)
{
    swapchain.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    renderer.clearValue.color = { {0.03f, 0.03f, 0.03f, 1.0f} };
    renderer.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    //scene.camera = Camera(glm::vec3(0.0f, 1.0f, 3.0f));
    //octree = Octree(Volume({-10,-10,-10},{10,10,10}), 16, 8);
}

Application::~Application()
{

}

void Application::onInit()
{
    SDL_SetRelativeMouseMode((SDL_bool)!paused);

    createPipelines();
    createTestScene();
    //createPlanetScene();
    createLights();
}

void Application::onCleanup()
{
    cleanupPipelines();
}

void Application::onResize(int, int)
{

}

void Application::draw(VkCommandBuffer cmd)
{
    auto extent = renderer.drawExtent();
    scene.camera.aspectRatio = (float)extent.width / (float)extent.height;

    // Update shader data
    scene.updateUniforms(cmd, pipelineLayout, currentFrameIndex);

    // bind once for all draw commands
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &scene.bindlessDescriptorSet, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, useClustered ? clusteredPbrPipeline : pbrPipeline);
    //vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    for (auto &model : scene.models.items) {
        model.draw(cmd, model.instances, currentFrameIndex, pipelineLayout, offsetof(BindlessPushConstants,instances));
    }

    if (showLights) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lightsPipeline);

        std::vector<vke::InstanceData> lightInstances;

        for (auto &light :  scene.lights) {
            float radius = light.radius;
            glm::mat4 t(1.0f);
            t = glm::translate(t, glm::vec3(light.position));
            t = glm::scale(t, glm::vec3(radius));
            lightInstances.push_back({
                .transform = light.type == LightType::Point ? t : glm::mat4(0.0f)
            });
        }

        auto &sphere = scene.models.get("sphere");
        sphere.draw(cmd, lightInstances, currentFrameIndex, pipelineLayout, offsetof(BindlessPushConstants,instances));
    }
}

void Application::drawUI()
{
    if (!paused)
        return;

    ImGui::Text("Frame time: %f milli", 1000.0f / ImGui::GetIO().Framerate);

    const char* availableSamples[] = { "Disabled", "MSAA 2x", "MSAA 4x", "MSAA 8x", "MSAA 16x" };
    static int currentSampleCount = 0;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    if (ImGui::Combo("MSAA", &currentSampleCount, availableSamples, IM_COUNTOF(availableSamples))) {
        switch (currentSampleCount) {
            case 0:
            samples = VK_SAMPLE_COUNT_1_BIT; break;
            case 1:
            samples = VK_SAMPLE_COUNT_2_BIT; break;
            case 2:
            samples = VK_SAMPLE_COUNT_4_BIT; break;
            case 3:
            samples = VK_SAMPLE_COUNT_8_BIT; break;
            case 4:
            samples = VK_SAMPLE_COUNT_16_BIT; break;
        }

        waitIdle();
            renderer.setMSAASamples(samples);
            cleanupPipelines();
            createPipelines();
        waitIdle();
    }

    float currentScale = renderer.renderScale;
    ImGui::SliderFloat("Render Scale: ", &renderer.renderScale, 0.1f, 1.0f);

    static float radius = 4.0f;
    static float intensity = 10.0f;
    static int _clusterGridSize[3];
    for (int i = 0; i < 3; ++i)
        _clusterGridSize[i] = clusterGridSize[i];
    ImGui::SliderInt3("Cluster Grid Size:", &_clusterGridSize[0], 1, 30);
    for (int i = 0; i < 3; ++i)
        clusterGridSize[i] = _clusterGridSize[i];
    ImGui::SliderFloat("Light Radius:", &radius, 0.1f, 20.0f);
    ImGui::SliderFloat("Light Intensity: ", &intensity, 0.1f, 50.0f);
    for (auto &light : scene.lights) {
        light.radius = radius;
        light.intensity = intensity;
    }
    ImGui::Checkbox("Clustered Shading:", &useClustered);
    ImGui::Checkbox("Show Clusters:", &showClusters);
    ImGui::Checkbox("Show Lights:", &showLights);

    if (currentScale != renderer.renderScale) {
        waitIdle();
            renderer.cleanup();
            renderer.createResources();
        waitIdle();
    }

    if (ImGui::SliderInt("Lights: ", &lightsCount, 1, 500)) {
        waitIdle();
        createLights();
        updateLights();
        waitIdle();
    }

    ImGui::Separator();

    vke::drawUI(scene);
}

void Application::update(float dt)
{
    if (!paused) {
        const Uint8 *keyState = SDL_GetKeyboardState(nullptr);

        if (keyState[SDL_SCANCODE_A]) scene.camera.processKeyboard(Camera::CameraMovement::LEFT, dt);
        if (keyState[SDL_SCANCODE_D]) scene.camera.processKeyboard(Camera::CameraMovement::RIGHT, dt);
        if (keyState[SDL_SCANCODE_W]) scene.camera.processKeyboard(Camera::CameraMovement::FORWARD, dt);
        if (keyState[SDL_SCANCODE_S]) scene.camera.processKeyboard(Camera::CameraMovement::BACKWARD, dt);
        if (keyState[SDL_SCANCODE_X]) scene.camera.processKeyboard(Camera::CameraMovement::UP, dt);
        if (keyState[SDL_SCANCODE_Z]) scene.camera.processKeyboard(Camera::CameraMovement::DOWN, dt);
    }

    //updatePlanetScene(dt);
    updateLights();
}

void Application::processEvent(const SDL_Event& e)
{
    static bool firstMouse = true;
    static float lastX = 0.0f, lastY = 0.0f;

    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
        SDL_SetRelativeMouseMode((SDL_bool)paused);
        //SDL_CaptureMouse((SDL_bool)!paused);
        paused = !paused;

        if (!paused) {
            firstMouse = true;
            lastX = 0.0f;
            lastY = 0.0f;
        }
    }

    if (paused)
        return;

    if (e.type == SDL_MOUSEWHEEL) {
        scene.camera.processMouseWheel(e.wheel.y);
    }

    if (e.type == SDL_MOUSEMOTION) {
        float xpos = e.motion.x;
        float ypos = e.motion.y;

        if (firstMouse) {
            lastX = xpos;               // Initialize previous position
            lastY = ypos;
            firstMouse = false;         // Disable special handling for subsequent calls
        }

        float xoffset = e.motion.xrel;  //xpos - lastX;
        float yoffset = -e.motion.yrel; //lastY - ypos;

        lastX = xpos;
        lastY = ypos;

        scene.camera.processMouseMovement(xoffset, yoffset, false);
    }

    if (e.type == SDL_KEYDOWN) {
        // TODO:
    }
}

void Application::createPipelines()
{
    // load shader modules
    VkShaderModule vertexShader = createShaderModule("res/shaders/mesh_ubo_world_lights.vert.spv", vulkan.device);
    VkShaderModule lightsVertexShader = createShaderModule("res/shaders/lights.vert.spv", vulkan.device);
    VkShaderModule mtlFragmentShader = createShaderModule("res/shaders/mesh_ubo_world_lights.frag.spv", vulkan.device);
    VkShaderModule pbrFragmentShader = createShaderModule("res/shaders/mesh_ubo_world_lights_pbr.frag.spv", vulkan.device);
    VkShaderModule clusteredPbrFragmentShader = createShaderModule("res/shaders/mesh_ubo_world_lights_pbr_clustered.frag.spv", vulkan.device);
    VkShaderModule lightsFragmentShader = createShaderModule("res/shaders/lights.frag.spv", vulkan.device);

    // 128 bytes (guaranteed minimum size)
    VkPushConstantRange range {
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = sizeof(BindlessPushConstants) // 4 buffers (camera, mesh, lights, materials)
    };

    VkPipelineLayoutCreateInfo layoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &scene.bindlessDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range
    };

    VK_CHECK(vkCreatePipelineLayout(vulkan.device, &layoutCreateInfo, nullptr, &pipelineLayout));

    std::vector<VkDynamicState> dynamicStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    PipelineBuilder pipelineBuilder;

    mtlPipeline = pipelineBuilder
        .setLayout(pipelineLayout)
        .addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
        .addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, mtlFragmentShader)
        .setVertexDescription(Vertex::description())
        .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        //.setPolygonMode(VK_POLYGON_MODE_LINE)
        .setPolygonMode(VK_POLYGON_MODE_FILL)
        .setDynamicStates(dynamicStates)
        .enableDepthTesting(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
        .setColorAttachmentFormat(swapchain.imageFormat)
        .setDepthAttachmentFormat(renderer.depthImage.info.format)
        .setMSAASamples(renderer.MSAASamples)
        .disableBlending()
        .build(vulkan.device, {});

    pipelineBuilder.shaderStages.clear();

    pbrPipeline = pipelineBuilder
        .addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
        .addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, pbrFragmentShader)
        .build(vulkan.device, {});

    pipelineBuilder.shaderStages.clear();

    clusteredPbrPipeline = pipelineBuilder
        .addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
        .addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, clusteredPbrFragmentShader)
        .build(vulkan.device, {});

    pipelineBuilder.shaderStages.clear();

    lightsPipeline = pipelineBuilder
        .addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, lightsVertexShader)
        .addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, lightsFragmentShader)
        .setPolygonMode(VK_POLYGON_MODE_LINE)
        .build(vulkan.device, {});

    // cleanup shader modules
    vkDestroyShaderModule(vulkan.device, vertexShader, nullptr);
    vkDestroyShaderModule(vulkan.device, lightsVertexShader, nullptr);
    vkDestroyShaderModule(vulkan.device, mtlFragmentShader, nullptr);
    vkDestroyShaderModule(vulkan.device, pbrFragmentShader, nullptr);
    vkDestroyShaderModule(vulkan.device, clusteredPbrFragmentShader, nullptr);
    vkDestroyShaderModule(vulkan.device, lightsFragmentShader, nullptr);
}

void Application::cleanupPipelines()
{
    vkDestroyPipeline(vulkan.device, lightsPipeline, nullptr);
    vkDestroyPipeline(vulkan.device, pbrPipeline, nullptr);
    vkDestroyPipeline(vulkan.device, clusteredPbrPipeline, nullptr);
    vkDestroyPipeline(vulkan.device, mtlPipeline, nullptr);
    vkDestroyPipelineLayout(vulkan.device, pipelineLayout, nullptr);
}

void Application::createTestScene()
{
    scene.camera = Camera(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f);
    scene.camera.nearPlane = 0.1f;
    scene.camera.farPlane = 30.0f;

    auto sponza = scene.loadGLTF("/home/crescoadmin/Projects/Vulkan/assets/models/sponza/sponza.gltf");
    sponza.instances.push_back({ .transform = glm::mat4(1.0f) });
    scene.storeModel(sponza);
}

void Application::createPlanetScene()
{
    scene.camera = Camera(glm::vec3(0.0f, 3.0f, 155.0f));
    scene.camera.nearPlane = 0.01;
    scene.camera.farPlane = 500.0f;

    auto planet = scene.loadModel("res/objects/gltf/lavaplanet.gltf");
    auto rock = scene.loadModel("res/objects/gltf/rock01.gltf");

    float radius = 150.0;
    float offset = 25.0f;

    glm::mat4 t = glm::mat4(1.0f);
    t = glm::translate(t, glm::vec3(0.0f, -3.0f, 0.0f));
    t = glm::scale(t, glm::vec3(8.0f, 8.0f, 8.0f));

    planet.instances.push_back({ .transform = t });

    for (uint32_t i = 0; i < rocksCount; ++i) {
        t = glm::mat4(1.0f);

        // 1. translation: displace along circle with 'radius' in range [-offset, offset]
        float angle = (float)i / (float)rocksCount * 360.0f;
        float displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
        float x = sin(angle) * radius + displacement;
        displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
        float y = displacement * 0.4f; // keep height of asteroid field smaller compared to width of x and z
        displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
        float z = cos(angle) * radius + displacement;
        t = glm::translate(t, glm::vec3(x, y, z));

        // 2. scale: Scale between 0.05 and 0.25f
        float scale = static_cast<float>((rand() % 20) / 50.0 + 1.0f);
        t = glm::scale(t, glm::vec3(scale));

        // 3. rotation: add random rotation around a (semi)randomly picked rotation axis vector
        float rotAngle = static_cast<float>((rand() % 360));
        t = glm::rotate(t, rotAngle, glm::vec3(0.4f, 0.6f, 0.8f));

        rock.instances.push_back({ .transform = t });
    }

    scene.storeModel(planet);
    rockModelIndex = scene.storeModel(rock);
}

void Application::createLights()
{
    scene.lights.clear();

    scene.lights.push_back({
        .direction = { -0.2f, -1.0f, -0.3f, 0.0f },
        .ambient = { 0.05f, 0.05f, 0.05f, 1.0f },
        .diffuse = { 0.8f, 0.8f, 0.8f, 1.0f },
        .specular = { 1.0f, 1.0f, 1.0f, 1.0f },
        .type = LightType::Directional,
    });

    /*
    scene.lights.push_back({
        .position = { camera.position, 1.0f },
        .direction = { camera.front, 1.0f },
        .ambient = { 0.0f, 0.0f, 0.0f, 1.0f },
        .diffuse = { 1.0f, 1.0f, 1.0f, 1.0f },
        .specular = { 1.0f, 1.0f, 1.0f, 1.0f },
        .constant = 1.0f,
        .linear = 0.09f,
        .quadratic = 0.032f,
        .cutOff = glm::cos(glm::radians(12.5f)),
        .outerCutOff = glm::cos(glm::radians(15.0f)),
        .type = LightType::Spot
    });*/

    default_random_engine generator;
    uniform_real_distribution<float> posDistribution(-10.0, 10.0);
    uniform_real_distribution<float> colorDistribution(0.0, 1.0);

    for (int i = 0; i < lightsCount; ++i) {
        glm::vec4 pos = { posDistribution(generator), posDistribution(generator), posDistribution(generator), 1.0f };
        glm::vec4 color = { colorDistribution(generator), colorDistribution(generator), colorDistribution(generator), 1.0f };
        scene.lights.push_back({
            .position = pos,
            .ambient = 0.1f * color,
            .diffuse = color,
            .specular = { 1.0f, 1.0f, 1.0f, 1.0f },
            .intensity = 10.0f,
            .radius = 4.0f,
            .constant = 1.0f,
            .linear = 20.0f,
            .quadratic = 13.8f,
            .type = LightType::Point
        });
    }

    if (!scene.models.contains("sphere")) {
        GeometryData sphereData = createSphere(1, 36, 18);
        auto sphere = scene.loadModel(sphereData.vertices, sphereData.indices, "sphere");
        scene.storeModel(sphere);
    }

    size_t lightSize = sizeof(uint32_t) * 4 + sizeof(Light) * scene.lights.size(); // (lights count + padding) + lights array
    scene.lightBuffers.create(framesInFlightCount, lightSize, vulkan.device, vulkan.allocator);
}

void Application::updatePlanetScene(float dt)
{
    auto &rockModel = scene.models.items[rockModelIndex];

    if (!pauseSimulation) {
        #pragma omp parallel for
        for (int i = 0; i < (int)rockModel.instances.size(); ++i) {
            auto &t = rockModel.instances[i].transform;
            auto pos = glm::vec3(t[3]);
            auto t1 = glm::translate(glm::mat4(1.0f),-pos);
            auto r = glm::rotate(glm::mat4(1.0f), glm::radians(dt), {0,1,0});
            auto t2 = glm::translate(glm::mat4(1.0f), pos);
            t = t1 * r * t2 * t;
        }
    }

    /*
    if (doCulling) {
        Frustum frustum(scene.camera.projection() * scene.camera.view());

        timeToCullInstances = measureExecution<chrono::microseconds>([&]{
            cullInstances(rockModel.instances, frustum);
        });

        culledInstances = 0;
        visibleRocks.clear();
        for (auto &instance : rockInstances) {
            if (instance.isVisible)
                visibleRocks.push_back(instance);
            else
                culledInstances++;
        }
    }*/
}

void Application::updateLights()
{
    /*
    // spotlight follows camera
    lights[2].position = glm::vec4(camera.position, 1.0f);
    lights[2].direction = glm::vec4(camera.front, 1.0f);

    uint32_t lightsCount = lights.size();
    lightDataBuffers.update(currentFrameIndex, &lightsCount, sizeof(uint32_t));
    lightDataBuffers.update(currentFrameIndex, lights.data(), sizeof(Light)*lights.size(), sizeof(uint32_t) * 4);

    for (int i = 0; i < (int)lightInstances.size(); ++i) {
        if (lights[i].type == LightType::Point)
            lightInstances[i].transform = glm::translate(glm::mat4(1.0f), glm::vec3(lights[i].position));
    }*/

    auto extent = renderer.drawExtent();

    // Update light clusters
    auto lightClusters = buildLightClusters(scene.camera.nearPlane, scene.camera.farPlane,
        clusterGridSize, glm::inverse(scene.camera.projection()));

    std::vector<vke::Sphere> lightSpheres(scene.lights.size());

    for (size_t i = 0; i < scene.lights.size(); ++i) {
        Light light = scene.lights[i];
        lightSpheres[i] = Sphere(scene.camera.view() * light.position, light.radius); //lightSphere(light, 0.001);
        if (light.type == LightType::Directional)
            lightSpheres[i].radius = FLT_MAX;
    }

    assignLightsToClusters(lightClusters, lightSpheres);

    scene.lightClusterBuffers.update(currentFrameIndex, lightClusters.data(), sizeof(LightCluster) * lightClusters.size());

    LightClusterInfo clusterInfo {
        .screenSize = { extent.width, extent.height },
        .clusterGridSize = clusterGridSize,
        .showClusters = showClusters,
        .zNear = scene.camera.nearPlane,
        .zFar = scene.camera.farPlane,
        .scale = clusterGridSize.z / std::log2(scene.camera.farPlane / scene.camera.nearPlane),
        .bias = -clusterGridSize.z * std::log2(scene.camera.nearPlane) / std::log2(scene.camera.farPlane / scene.camera.nearPlane)
    };

    scene.lightClusterInfoBuffers.update(currentFrameIndex, &clusterInfo, sizeof(LightClusterInfo));
}

/*
void Application::cullInstances(std::vector<InstanceData> &instances, const Frustum &frustum)
{
    #pragma omp parallel for
    for (auto &instance : instances) {
        Volume v = rock.volume.transformed(instance.transform);
        instance.isVisible = frustum.intersect(v.min, v.max);
    }
}*/
