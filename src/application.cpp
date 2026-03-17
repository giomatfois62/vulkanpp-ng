#include "application.hpp"
#include "vk_pipeline.hpp"
#include "vk_utils.hpp"
#include "vk_geometry.hpp"

#include "imgui.h"
#include <random>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <iostream>

using namespace std;
using namespace vke;

Application::Application(int argc, char **argv) :
	Engine(argc, argv)
{
    //clearValue.color = { {0.03f, 0.03f, 0.03f, 1.0f} };
    clearValue.color = { {0.00f, 0.00f, 0.00f, 1.0f} };

    //camera = Camera(glm::vec3(0.0f, 1.0f, 3.0f));
    camera = Camera(glm::vec3(0.0f, 1.0f, 155.0f));

    octree = Octree(Volume({-10,-10,-10},{10,10,10}), 16, 8);
}

Application::~Application()
{

}

void Application::onInit()
{
    SDL_SetRelativeMouseMode((SDL_bool)!paused);

    createPipelines();
    loadAssets();
}

void Application::onCleanup()
{
	cleanupPipelines();
    cleanupAssets();
}

void Application::onResize(int, int)
{

}

void Application::draw(VkCommandBuffer cmd)
{
    // Update shader data
    sceneData.projection = camera.projection((float)drawExtent().width / (float)drawExtent().height, 0.1f, 1000.0f);
    sceneData.view = camera.view();
    sceneData.viewPos = { camera.position, 1.0f };
    sceneDataBuffers.update(currentFrameIndex, &sceneData, sizeof(sceneData));
    materialBuffers.update(currentFrameIndex, assets.materials.data(), assets.materials.dataSize());
    pbrMaterialBuffers.update(currentFrameIndex, assets.pbrMaterials.data(), assets.pbrMaterials.dataSize());

    // begin dynamic rendering
    /*
    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = msaaEnabled() ? colorImage.view : swapchain.imageViews[currentImageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = msaaEnabled() ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
        .resolveImageView = msaaEnabled() ? swapchain.imageViews[currentImageIndex] : VK_NULL_HANDLE,
        .resolveImageLayout = msaaEnabled() ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, //VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearValue
    };*/
    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = msaaEnabled() ? colorImage.view : drawImage.view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = msaaEnabled() ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
        .resolveImageView = msaaEnabled() ? drawImage.view : VK_NULL_HANDLE,
        .resolveImageLayout = msaaEnabled() ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        // on nvidia ".storeOp = OP_DONT_CARE" doesn't work without MSAA
        .storeOp = msaaEnabled() ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearValue
    };

    VkRenderingAttachmentInfo depthAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depthImage.view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = VkClearValue{ .depthStencil{ 1.0f, 0 } }
    };

    auto extent = drawExtent();

    VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        //.renderArea = { .extent = windowSize },
        .renderArea = { .extent = extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo,
        .pDepthAttachment = &depthAttachmentInfo
    };

    vkCmdBeginRendering(cmd, &renderInfo);

    // bind once for all draw commands
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &bindlessDescriptorSet, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pbrPipeline);
    //vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // set pipeline dynamic states
    VkViewport viewport{
        0.0f ,
        static_cast<float>(extent.height),
        static_cast<float>(extent.width),
        -static_cast<float>(extent.height), // negative height to conform to opengl Y up
        //static_cast<float>(windowSize.height),
        //static_cast<float>(windowSize.width),
        //-static_cast<float>(windowSize.height), // negative height to conform to opengl Y up
        0.0f,
        1.0f
    };

    //VkRect2D scissor{ VkOffset2D{ 0, 0 }, windowSize };
    VkRect2D scissor{ VkOffset2D{ 0, 0 }, extent };
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(VkDeviceAddress), &sceneDataBuffers.buffers[currentFrameIndex].address);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, sizeof(VkDeviceAddress) * 2, sizeof(VkDeviceAddress), &pbrMaterialBuffers.buffers[currentFrameIndex].address);
    //vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, sizeof(VkDeviceAddress) * 2, sizeof(VkDeviceAddress), &materialBuffers.buffers[currentFrameIndex].address);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, sizeof(VkDeviceAddress) * 3, sizeof(VkDeviceAddress), &lightDataBuffers.buffers[currentFrameIndex].address);

    //drawTestScene(cmd, pipelineLayout);
    drawPlanetScene(cmd, pipelineLayout);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lightsPipeline);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(VkDeviceAddress), &sceneDataBuffers.buffers[currentFrameIndex].address);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, sizeof(VkDeviceAddress) * 2, sizeof(VkDeviceAddress), &materialBuffers.buffers[currentFrameIndex].address);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, sizeof(VkDeviceAddress) * 3, sizeof(VkDeviceAddress), &lightDataBuffers.buffers[currentFrameIndex].address);

    sphere.draw(cmd, lightInstances, currentFrameIndex, pipelineLayout);

    vkCmdEndRendering(cmd);

    changeImageLayout(cmd, drawImage.handle,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );

    changeImageLayout(cmd, swapchain.images[currentImageIndex],
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // if rendering offscreen
        //VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // if rendering or resolving directly on it
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );

    copyImageToImage(cmd, drawImage.handle, swapchain.images[currentImageIndex], extent, swapchain.extent, VK_FILTER_LINEAR);

    changeImageLayout(cmd, swapchain.images[currentImageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );
}

void Application::drawUI()
{
    //ImGui::ShowDemoWindow();
    if (!paused)
        return;

    ImGui::Text("ImGui Events Time: %lf micro", benchmarks.imguiEventsTime);
    ImGui::Text("App Events Time: %lf micro", benchmarks.appEventsTime);
    ImGui::Text("Rendering Time: %f milli", lagInMillisecs);
    ImGui::Text("Rendering Time (ImGui): %f milli", 1000.0f / ImGui::GetIO().Framerate);

    ImGui::Text("Time to build clusters: %lf micro", timeToBuildClusters);
    ImGui::Text("Time to assign lights: %lf micro", timeToAssignLights);
    ImGui::Text("Time to cull instances: %lf micro", timeToCullInstances);
    ImGui::Text("Culled instances: %ld", culledInstances);
    ImGui::Checkbox("Cull instances:", &doCulling);

    const char* availableSamples[] = { "Disabled", "MSAA 2x", "MSAA 4x", "MSAA 8x", "MSAA 16x" };
    static int current = 0;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    if (ImGui::Combo("MSAA", &current, availableSamples, IM_COUNTOF(availableSamples))) {
        switch (current) {
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

        MSAASamples = samples;
        recreateSwapchain();
        cleanupPipelines();
        createPipelines();
    }
    float currentScale = renderScale;
    ImGui::SliderFloat("Render Scale: ", &renderScale, 0.1f, 1.0f);
    if (currentScale != renderScale) {
        waitIdle();
            cleanupRenderingResources();
            createRenderingResources();
        waitIdle();
    }

    ImGui::SliderFloat4("Light", &sceneData.light[0], -10, 10);
    ImGui::SliderFloat3("Model Position", &objPosition[0], -10, 10);
    ImGui::SliderFloat3("Model Rotation", &objRotation[0], -10, 10);
    ImGui::SliderFloat("Model Scale", &objScale, 0, 3);
    ImGui::Separator();        /*
        float modelZ = 0.0f;

        for (int i = 0; i < (int)mesh.drawData.size(); ++i) {
            glm::vec3 pos = objPosition + glm::vec3((float)((i%5) - 2) * 3.0f, -0.f, modelZ);
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(glm::quat(objRotation));
            transform = glm::scale(transform, glm::vec3(objScale));
            mesh.drawData[i].transform = transform;

            if (i%5 == 0) {
                modelZ -= 2;
            }
        }*/
    ImGui::SliderFloat("Speed", &camera.movementSpeed, 0, 100);
    ImGui::SliderFloat("Sensitivity", &camera.mouseSensitivity, 0, 1);
    ImGui::Separator();

    std::vector<std::string> materialsStr(assets.materials.items.size());
    std::vector<const char*> materials(assets.materials.items.size());
    for (size_t i = 0; i < materialsStr.size(); ++i) {
        std::string name = "Material_" + to_string(i);
        materialsStr[i] = name.c_str();
        materials[i] = materialsStr[i].c_str();
    }

    /*
    static int selectedMaterial = model.meshes[0].material;
    ImGui::Combo("Material", &selectedMaterial, materials.data(), materials.size());
    ImGui::ColorEdit3("Diffuse:", &assets.materials.items[selectedMaterial].diffuse[0]);
    ImGui::ColorEdit3("Specular:", &assets.materials.items[selectedMaterial].specular[0]);
    ImGui::SliderFloat("Shininess:", &assets.materials.items[selectedMaterial].shininess, 1, 512);

    static bool useNormalMap = true;
    if (ImGui::Checkbox("Normal Map:", &useNormalMap)) {
        if (useNormalMap) {
            assets.materials.items = backupMaterials;
        } else {
            for (auto &material : assets.materials.items) {
                material.bumpTex = 0;
            }
        }
    }

    if (ImGui::Button("Assign")) {
        for (auto &mesh : model.meshes) {
            for (auto &inst : mesh.drawData)
                inst.material = selectedMaterial;
        }
    }
    ImGui::Separator();*/

    ImGui::SliderFloat3("DirLight Direction:", &lights[0].direction[0], -1, 1);
    ImGui::ColorEdit3("DirLight Ambient:", &lights[0].ambient[0]);
    ImGui::ColorEdit3("DirLight Diffuse:", &lights[0].diffuse[0]);
    ImGui::ColorEdit3("DirLight Specular:", &lights[0].specular[0]);
    ImGui::Separator();

    ImGui::SliderFloat3("PointLight Position:", &lights[1].position[0], -5, 5);
    ImGui::ColorEdit3("PointLight Ambient:", &lights[1].ambient[0]);
    ImGui::ColorEdit3("PointLight Diffuse:", &lights[1].diffuse[0]);
    ImGui::ColorEdit3("PointLight Specular:", &lights[1].specular[0]);
    ImGui::SliderFloat("PointLight Constant:", &lights[1].constant, 0, 1);
    ImGui::SliderFloat("PointLight Linear:", &lights[1].linear, 0, 1);
    ImGui::SliderFloat("PointLight Quadratic:", &lights[1].quadratic, 0, 1);
    ImGui::Separator();

    ImGui::ColorEdit3("SpotLight Ambient:", &lights[2].ambient[0]);
    ImGui::ColorEdit3("SpotLight Diffuse:", &lights[2].diffuse[0]);
    ImGui::ColorEdit3("SpotLight Specular:", &lights[2].specular[0]);
    ImGui::SliderFloat("SpotLight Constant:", &lights[2].constant, 0, 1);
    ImGui::SliderFloat("SpotLight Linear:", &lights[2].linear, 0, 1);
    ImGui::SliderFloat("SpotLight Quadratic:", &lights[2].quadratic, 0, 1);
    ImGui::SliderFloat("SpotLight CutOff:", &lights[2].cutOff, 0, 1);
    ImGui::SliderFloat("SpotLight OuterCutOff:", &lights[2].outerCutOff, 0, 1);
}

void Application::update(float dt)
{
    if (!paused) {
        const Uint8 *keyState = SDL_GetKeyboardState(nullptr);

        if (keyState[SDL_SCANCODE_A]) camera.processKeyboard(Camera::CameraMovement::LEFT, dt);
        if (keyState[SDL_SCANCODE_D]) camera.processKeyboard(Camera::CameraMovement::RIGHT, dt);
        if (keyState[SDL_SCANCODE_W]) camera.processKeyboard(Camera::CameraMovement::FORWARD, dt);
        if (keyState[SDL_SCANCODE_S]) camera.processKeyboard(Camera::CameraMovement::BACKWARD, dt);
        if (keyState[SDL_SCANCODE_X]) camera.processKeyboard(Camera::CameraMovement::UP, dt);
        if (keyState[SDL_SCANCODE_Z]) camera.processKeyboard(Camera::CameraMovement::DOWN, dt);
    }

    //updateTestScene(dt);
    updatePlanetScene(dt);
    updateLights(dt);
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
        camera.processMouseWheel(e.wheel.y);
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

        camera.processMouseMovement(xoffset, yoffset, false);
    }

    if (e.type == SDL_KEYDOWN) {
        // TODO:
    }
}

void Application::createPipelines()
{
    VkShaderModule meshVertexShader = createShaderModule("res/shaders/mesh_ubo_world_lights.vert.spv", vulkan.device);
    VkShaderModule coloredFragmentShader = createShaderModule("res/shaders/mesh_ubo_world_lights.frag.spv", vulkan.device);

    if (meshVertexShader == VK_NULL_HANDLE)
        cerr << "Error when building the mesh vertex shader module" << endl;

    if (coloredFragmentShader == VK_NULL_HANDLE)
        cerr << "Error when building the mesh fragment shader module" << endl;

    // 128 bytes (guaranteed minimum size)
    VkPushConstantRange range {
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = sizeof(VkDeviceAddress) * 4 // 4 buffers (camera, mesh, lights, materials)
    };

    VkPipelineLayoutCreateInfo layoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &bindlessDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range
    };

    VK_CHECK(vkCreatePipelineLayout(vulkan.device, &layoutCreateInfo, nullptr, &pipelineLayout));

    std::vector<VkDynamicState> dynamicStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    pipeline = PipelineBuilder()
        .setLayout(pipelineLayout)
        .addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, meshVertexShader)
        .addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, coloredFragmentShader)
        .setVertexDescription(Vertex::description())
        .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        //.setPolygonMode(VK_POLYGON_MODE_LINE)
        .setPolygonMode(VK_POLYGON_MODE_FILL)
        .setDynamicStates(dynamicStates)
        .enableDepthTesting(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
        .setColorAttachmentFormat(swapchain.imageFormat)
        .setDepthAttachmentFormat(depthImage.info.format)
        .setMSAASamples(MSAASamples)
        .disableBlending()
        .build(vulkan.device, {});

    // cleanup shader modules
    vkDestroyShaderModule(vulkan.device, meshVertexShader, nullptr);
    vkDestroyShaderModule(vulkan.device, coloredFragmentShader, nullptr);

    meshVertexShader = createShaderModule("res/shaders/lights.vert.spv", vulkan.device);
    coloredFragmentShader = createShaderModule("res/shaders/lights.frag.spv", vulkan.device);

    lightsPipeline = PipelineBuilder()
            .setLayout(pipelineLayout)
            .addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, meshVertexShader)
            .addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, coloredFragmentShader)
            .setVertexDescription(Vertex::description())
            .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            //.setPolygonMode(VK_POLYGON_MODE_LINE)
            .setPolygonMode(VK_POLYGON_MODE_FILL)
            .setDynamicStates(dynamicStates)
            .enableDepthTesting(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
            .setColorAttachmentFormat(swapchain.imageFormat)
            .setDepthAttachmentFormat(depthImage.info.format)
            .setMSAASamples(MSAASamples)
            .disableBlending()
            .build(vulkan.device, {});

    vkDestroyShaderModule(vulkan.device, meshVertexShader, nullptr);
    vkDestroyShaderModule(vulkan.device, coloredFragmentShader, nullptr);

    createPBRPipeline();
    createOffscreenPipeline();
}

void Application::createPBRPipeline()
{
    VkShaderModule vertexShader = createShaderModule("res/shaders/mesh_ubo_world_lights.vert.spv", vulkan.device);
    VkShaderModule fragmentShader = createShaderModule("res/shaders/mesh_ubo_world_lights_pbr.frag.spv", vulkan.device);

    if (vertexShader == VK_NULL_HANDLE)
        cerr << "Error when building the mesh vertex shader module" << endl;

    if (fragmentShader == VK_NULL_HANDLE)
        cerr << "Error when building the mesh fragment shader module" << endl;

    std::vector<VkDynamicState> dynamicStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    pbrPipeline = PipelineBuilder()
        .setLayout(pipelineLayout)
        .addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
        .addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader)
        .setVertexDescription(Vertex::description())
        .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        //.setPolygonMode(VK_POLYGON_MODE_LINE)
        .setPolygonMode(VK_POLYGON_MODE_FILL)
        .setDynamicStates(dynamicStates)
        .enableDepthTesting(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
        .setColorAttachmentFormat(swapchain.imageFormat)
        .setDepthAttachmentFormat(depthImage.info.format)
        .setMSAASamples(MSAASamples)
        .disableBlending()
        .build(vulkan.device, {});

    // cleanup shader modules
    vkDestroyShaderModule(vulkan.device, vertexShader, nullptr);
    vkDestroyShaderModule(vulkan.device, fragmentShader, nullptr);
}

void Application::createOffscreenPipeline()
{
    VkShaderModule meshVertexShader = createShaderModule("res/shaders/fullscreen_triangle.vert.spv", vulkan.device);
    VkShaderModule coloredFragmentShader = createShaderModule("res/shaders/fullscreen_triangle.frag.spv", vulkan.device);

    if (meshVertexShader == VK_NULL_HANDLE)
        cerr << "Error when building the mesh vertex shader module" << endl;

    if (coloredFragmentShader == VK_NULL_HANDLE)
        cerr << "Error when building the mesh fragment shader module" << endl;

    // 128 bytes (guaranteed minimum size)
    VkPushConstantRange range {
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = sizeof(VkDeviceAddress) * 3
    };

    VkPipelineLayoutCreateInfo layoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &bindlessDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range
    };

    VK_CHECK(vkCreatePipelineLayout(vulkan.device, &layoutCreateInfo, nullptr, &offscreenPipelineLayout));

    std::vector<VkDynamicState> dynamicStates{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    offscreenPipeline = PipelineBuilder()
        .setLayout(offscreenPipelineLayout)
        .addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, meshVertexShader)
        .addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, coloredFragmentShader)
        //.setVertexDescription(Vertex::description())
        .setCullMode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        //.setPolygonMode(VK_POLYGON_MODE_LINE)
        .setPolygonMode(VK_POLYGON_MODE_FILL)
        .setDynamicStates(dynamicStates)
        //.enableDepthTesting(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL)
        .setColorAttachmentFormat(swapchain.imageFormat)
        //.setDepthAttachmentFormat(depthImage.info.format)
        .setMSAASamples(VK_SAMPLE_COUNT_1_BIT)
        //.setMSAASamples(MSAASamples)
        .disableBlending()
        .build(vulkan.device, {});

    // cleanup shader modules
    vkDestroyShaderModule(vulkan.device, meshVertexShader, nullptr);
    vkDestroyShaderModule(vulkan.device, coloredFragmentShader, nullptr);
}

void Application::cleanupPipelines()
{
    vkDestroyPipeline(vulkan.device, lightsPipeline, nullptr);
    vkDestroyPipeline(vulkan.device, pbrPipeline, nullptr);
    vkDestroyPipeline(vulkan.device, pipeline, nullptr);
    vkDestroyPipelineLayout(vulkan.device, pipelineLayout, nullptr);

    vkDestroyPipeline(vulkan.device, offscreenPipeline, nullptr);
    vkDestroyPipelineLayout(vulkan.device, offscreenPipelineLayout, nullptr);
}

void Application::loadAssets()
{
    //loadTestScene();
    loadPlanetScene();
    loadLights();

    backupMaterials = assets.materials.items;
    sceneDataBuffers.create(framesInFlight.size(), sizeof(sceneData), vulkan.device, vulkan.allocator);
    materialBuffers.create(framesInFlight.size(), assets.materials.dataSize(), vulkan.device, vulkan.allocator);
    pbrMaterialBuffers.create(framesInFlight.size(), assets.pbrMaterials.dataSize(), vulkan.device, vulkan.allocator);
}

void Application::cleanupAssets()
{
    model.cleanup();
    sphere.cleanup();
    planet.cleanup();
    rock.cleanup();

    sceneDataBuffers.cleanup();
    pbrMaterialBuffers.cleanup();
    materialBuffers.cleanup();
    lightDataBuffers.cleanup();
}

void Application::loadTestScene()
{
    //model = loadOBJ("res/objects/suzanne/suzanne.obj");
    //model = loadOBJ("res/objects/planet/planet.obj");
    //model = loadOBJ("res/objects/cyborg/cyborg.obj");
    //model = loadOBJ("res/objects/backpack/backpack.obj");
    //model = loadOBJ("res/objects/plant_on_table/plant_on_table.obj");
    //model = loadOBJ("res/objects/nanosuit/nanosuit.obj");
    //objPosition.y = -3.0f;
    //objScale = 0.25;

    //model = loadGLTF("/home/crescoadmin/Projects/Vulkan/assets/models/FlightHelmet/glTF/FlightHelmet.gltf");
    //model = loadGLTF("res/objects/gltf/voyager.gltf");
    //objScale = 0.1f;
    //model = loadGLTF("res/objects/gltf/voyager.gltf");
    model = loadGLTF("/home/crescoadmin/Projects/Vulkan/assets/models/sponza/sponza.gltf");
    objScale = 0.1;
    //model = loadGLTF("res/objects/gltf/deer.gltf");
    //model = loadGLTF("res/objects/gltf/torusknot.gltf");

    //model = Model({ createCube() });
    //objPosition.z = 10.0f;
    //model = { .meshes = { createSphere(0.5, 36, 18) } };
    //model = { .meshes = { createCylinder(0.5, 1, 32) } };
    //model = { .meshes = { createTorus(1.0f, 0.5f, 36, 18) } };
    //model = { .meshes = { createCone(1.0f, 2.5f, 8, 1) } };

    for (uint32_t i = 0; i < instances; ++i) {
        modelInstances.push_back({ .transform = glm::mat4(1.0f) });
    }

    assets.materials.items[0].normalTex = loadTexture("res/textures/brickwall_normal.jpg", VK_FORMAT_R8G8B8A8_UNORM);
    assets.materials.items[0].diffuseTex = loadTexture("res/textures/brickwall.jpg");
}

void Application::loadPlanetScene()
{
    //planet = loadOBJ("res/objects/planet/planet.obj");
    //rock = loadOBJ("res/objects/rock/rock.obj");
    planet = loadModel("res/objects/gltf/lavaplanet.gltf");
    rock = loadModel("res/objects/gltf/rock01.gltf");

    float radius = 150.0;
    float offset = 25.0f;

    glm::mat4 t = glm::mat4(1.0f);
    t = glm::translate(t, glm::vec3(0.0f, -3.0f, 0.0f));
    t = glm::scale(t, glm::vec3(8.0f, 8.0f, 8.0f));

    planetInstances.push_back({ .transform = t });

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

        rockInstances.push_back({ .transform = t });
    }
}

void Application::loadLights()
{
    lights.push_back({
        .direction = { -0.2f, -1.0f, -0.3f, 0.0f },
        .ambient = { 0.05f, 0.05f, 0.05f, 1.0f },
        .diffuse = { 0.8f, 0.8f, 0.8f, 1.0f },
        .specular = { 1.0f, 1.0f, 1.0f, 1.0f },
        .type = LightType::Directional,
    });

    lights.push_back({
        .position = { 0.0f, 1.0f, 1.0f , 1.0f},
        .ambient = { 0.05f, 0.05f, 0.05f, 1.0f },
        .diffuse = { 0.25f, 0.0f, 1.0f, 1.0f },
        .specular = { 1.0f, 1.0f, 1.0f, 1.0f },
        .constant = 1.0f,
        .linear = 0.09f,
        .quadratic = 0.032f,
        .type = LightType::Point
    });

    lights.push_back({
        .position = {camera.position, 1.0f},
        .direction = {camera.front, 1.0f},
        .ambient = { 0.0f, 0.0f, 0.0f, 1.0f },
        .diffuse = { 1.0f, 1.0f, 1.0f, 1.0f },
        .specular = { 1.0f, 1.0f, 1.0f, 1.0f },
        .constant = 1.0f,
        .linear = 0.09f,
        .quadratic = 0.032f,
        .cutOff = glm::cos(glm::radians(12.5f)),
        .outerCutOff = glm::cos(glm::radians(15.0f)),
        .type = LightType::Spot
    });

    default_random_engine gen;
    uniform_real_distribution<float> distribution(-10.0, 10.0);
    uniform_real_distribution<float> distribution2(0.0, 1.0);

    for (uint32_t i = 0; i < lightsCount; ++i) {
        glm::vec4 pos = { distribution(gen), 1, distribution(gen), 1.0f };
        glm::vec4 col = { distribution2(gen), distribution2(gen), distribution2(gen), 1.0f };
        lights.push_back({
            .position = pos,
            .ambient = 0.1f * col,
            .diffuse = col,
            .specular = { 1.0f, 1.0f, 1.0f, 1.0f },
            .constant = 1.0f,
            .linear = 20.0f,
            .quadratic = 15.8f,
            .type = LightType::Point
        });
    }

    size_t lightSize = sizeof(uint32_t) * 4 + sizeof(Light) * lights.size(); // (lights count + padding) + lights array
    lightDataBuffers.create(framesInFlight.size(), lightSize, vulkan.device, vulkan.allocator);

    GeometryData sphereData = createSphere(0.2, 36, 18);
    sphere = loadModel(sphereData.vertices, sphereData.indices);

    for (uint32_t i = 0; i < lights.size(); ++i) {
        if (lights[i].type == LightType::Point) {
            vke::Sphere lightVol = lightSphere(lights[i]);
            std::cout << "light[" << i<<"] radius: " << lightVol.radius << std::endl;
            lightInstances.push_back({
                .transform = glm::translate(glm::mat4(1.0f), glm::vec3(lights[i].position))
            });
        } else {
            lightInstances.push_back({});
        }
    }
}

void Application::updateTestScene(float dt)
{
    float modelZ = 0.0f;

    for (int i = 0; i < (int)modelInstances.size(); ++i) {
        glm::vec3 pos = objPosition + glm::vec3((float)((i%5) - 2) * 3.0f, -0.f, modelZ);
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(glm::quat(objRotation));
        modelInstances[i].transform = glm::scale(transform, glm::vec3(objScale));

        if (i%5 == 0) {
            modelZ -= 2;
        }
    }
}

void Application::updatePlanetScene(float dt)
{
    #pragma omp parallel for
    for (int i = 0; i < (int)rockInstances.size(); ++i) {
        auto &t = rockInstances[i].transform;
        auto pos = glm::vec3(t[3]);
        auto t1 = glm::translate(glm::mat4(1.0f),-pos);
        auto r = glm::rotate(glm::mat4(1.0f), glm::radians(dt), {0,1,0});
        auto t2 = glm::translate(glm::mat4(1.0f), pos);
        t = t1 * r * t2 * t;
    }

    if (doCulling) {
        Frustum frustum(camera.projection((float)drawExtent().width / (float)drawExtent().height, 0.1f, 1000.0f) * camera.view());

        timeToCullInstances = measureExecution<chrono::microseconds>([&]{
            cullInstances(rockInstances, frustum);
        });

        culledInstances = 0;
        visibleRocks.clear();
        for (auto &instance : rockInstances) {
            if (instance.isVisible)
                visibleRocks.push_back(instance);
            else
                culledInstances++;
        }
    }
}

void Application::updateLights(float dt)
{
    // spotlight follows camera
    lights[2].position = glm::vec4(camera.position, 1.0f);
    lights[2].direction = glm::vec4(camera.front, 1.0f);

    uint32_t lightsCount = lights.size();
    lightDataBuffers.update(currentFrameIndex, &lightsCount, sizeof(uint32_t));
    lightDataBuffers.update(currentFrameIndex, lights.data(), sizeof(Light)*lights.size(), sizeof(uint32_t) * 4);

    for (int i = 0; i < (int)lightInstances.size(); ++i) {
        if (lights[i].type == LightType::Point)
            lightInstances[i].transform = glm::translate(glm::mat4(1.0f), glm::vec3(lights[i].position));
    }

    // Update light clusters
    timeToBuildClusters = measureExecution<std::chrono::microseconds>([&]{
        auto proj = camera.projection((float)drawExtent().width / (float)drawExtent().height, 0.1f, 100.0f);
        lightClusters = buildLightClusters(0.1f, 100.0f, { 16, 9, 24 }, { drawExtent().width, drawExtent().height }, glm::inverse(proj));
    });

    timeToAssignLights = measureExecution<std::chrono::microseconds>([&]{
        std::vector<vke::Sphere> lightSpheres(lights.size());
        for (size_t i = 0; i < lights.size(); ++i)
            lightSpheres[i] = lightSphere(lights[i]);

        assignLightsToClusters(lightClusters, lightSpheres);
    });
}

void Application::drawTestScene(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    model.draw(cmd, modelInstances, currentFrameIndex, layout);
}

void Application::drawPlanetScene(VkCommandBuffer cmd, VkPipelineLayout layout)
{
    planet.draw(cmd, planetInstances, currentFrameIndex, layout);
    rock.draw(cmd, doCulling ? visibleRocks : rockInstances, currentFrameIndex, layout);
}

void Application::cullInstances(std::vector<InstanceData> &instances, const Frustum &frustum)
{
    #pragma omp parallel for
    for (auto &instance : instances) {
        Volume v = rock.volume.transformed(instance.transform);
        instance.isVisible = frustum.intersect(v.min, v.max);
    }
}
