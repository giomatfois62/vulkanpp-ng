#include "vk_engine.hpp"
#include "vk_utils.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include <SDL_vulkan.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace std {
    template<> struct hash<vke::Vertex> {
        size_t operator()(vke::Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
               (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
               (hash<glm::vec2>()(vertex.uv) << 1) ^
               (hash<uint32_t>()(vertex.material) << 1);
        }
    };
}

using namespace std;
using namespace vke;

Engine::Engine(int, char**)
{

}

void Engine::run()
{
	init();
    loop();
	cleanup();
}

void Engine::quit()
{
	shouldQuit = true;
}

void Engine::setWindowTitle(const char *title)
{
	windowTitle = title;

	if (window)
		SDL_SetWindowTitle(window, title);
}

void Engine::setApplicationName(const char *name)
{
    vulkan.applicationName = name;
}

void Engine::init()
{
	createWindow();
    createVulkan();
    createSwapchain();
    createDrawImage();
    createColorResources();
    createDepthResources();
    //createDefaultRenderPass();
    //createFrameBuffers();
    createFrameObjects();
    createBindlessDescriptors();
    createScene();
    initImGui();

	onInit();
}

void Engine::loop()
{
	shouldQuit = false;

	auto start = std::chrono::high_resolution_clock::now();
	auto last = start;
	size_t frameCounter = 0;

	while (!shouldQuit) {
		auto end = std::chrono::high_resolution_clock::now();
		lagInMillisecs = std::chrono::duration<float, std::milli>(end - start).count();
		start = end;

		float dt = lagInMillisecs * 0.001f;
		float fpsTimer = std::chrono::duration<float,std::milli>(end - last).count();

		if (fpsTimer > 1000.0f) {
			uint32_t fps = static_cast<uint32_t>(frameCounter * (1000.0f / fpsTimer));

            std::string title = vulkan.applicationName + std::string(" - ") + std::to_string(fps) + "fps";
			setWindowTitle(title.c_str());

			frameCounter = 0;
			last = start;
		}

		processEvents();

		update(dt);

        if (!isHidden) {
            updateImGui();
            renderFrame();
        }

        frameCounter++;
	}

	// wait for pending operations
	waitIdle();
}

void Engine::cleanup()
{
	onCleanup();

    scene.cleanup();

    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(vulkan.device, imguiDescriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(vulkan.device, bindlessDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(vulkan.device, descriptorPool, nullptr);

    for (auto &frame : framesInFlight) {
        vkDestroySemaphore(vulkan.device, frame.imageAvailableSemaphore, nullptr);
        vkDestroyFence(vulkan.device, frame.renderFinishedFence, nullptr);

        vkFreeCommandBuffers(vulkan.device, frame.commandPool, 1, &frame.mainCommandBuffer);
        vkDestroyCommandPool(vulkan.device, frame.commandPool, nullptr);
	}

    for (auto &semaphore: renderFinishedSemaphores) {
        vkDestroySemaphore(vulkan.device, semaphore, nullptr);
    }

    //for (auto &frameBuffer : frameBuffers)
    //    vkDestroyFramebuffer(vulkan.device, frameBuffer, nullptr);

    //vkDestroyRenderPass(vulkan.device, renderPass, nullptr);

    cleanupRenderingResources();

    swapchain.cleanup();

    vulkan.cleanup();

	SDL_DestroyWindow(window);
	window = nullptr;
}

void Engine::processEvents()
{
    SDL_Event e;

	while (SDL_PollEvent(&e) != 0) {
		if (e.type == SDL_QUIT)
			quit();

		if (e.type == SDL_WINDOWEVENT) {
			if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
				int width = e.window.data1;
				int height = e.window.data2;

                resize(width, height);
			}

            if (e.window.event == SDL_WINDOWEVENT_MINIMIZED || e.window.event == SDL_WINDOWEVENT_HIDDEN) {
                isHidden = true;
            }

            if (e.window.event == SDL_WINDOWEVENT_RESTORED || e.window.event == SDL_WINDOWEVENT_SHOWN) {
                isHidden = false;
            }
		}

		if (e.type == SDL_KEYDOWN) {
			if (e.key.keysym.sym == SDLK_f) {
				float dt = lagInMillisecs * 0.001f;
				float fps = 1.0f / dt;

				cout << "FPS: " << fps << endl;
			}
		}

        benchmarks.imguiEventsTime = measureExecution<std::chrono::microseconds>([&]{
            ImGui_ImplSDL2_ProcessEvent(&e);
        });

        // return if io.WantCaptureMouse or io.WantCaptureKeyboard is true
        if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && ImGui::GetIO().WantCaptureKeyboard)
            return;

        /*
        if ((e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEWHEEL || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) &&
            ImGui::GetIO().WantCaptureMouse)
            return;*/

        benchmarks.appEventsTime = measureExecution<std::chrono::microseconds>([&]{
            processEvent(e);
        });

        //SDL_WaitEventTimeout(nullptr, 10);
	}
}

void Engine::resize(int width, int height)
{
	windowSize.width = width;
	windowSize.height = height;

	recreateSwapchain();

	onResize(width, height);
}

void Engine::renderFrame()
{
    Frame currentFrame = framesInFlight[currentFrameIndex];

	if (!prepareFrame(currentFrame))
		return;

    VkCommandBuffer cmd = currentFrame.mainCommandBuffer;

    beginDraw(cmd);
    draw(cmd);
    drawImGui(cmd);
    endDraw(cmd);

	presentFrame(currentFrame);

	currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Engine::beginDraw(VkCommandBuffer cmd)
{
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo cmdBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    changeImageLayout(cmd, drawImage.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // vkguide?
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );

    changeImageLayout(cmd, colorImage.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );

    changeImageLayout(cmd, depthImage.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, .levelCount = 1, .layerCount = 1 }
    );
}

void Engine::endDraw(VkCommandBuffer cmd)
{
    changeImageLayout(cmd, swapchain.images[currentImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );

    VK_CHECK(vkEndCommandBuffer(cmd));
}

void Engine::waitIdle()
{
    vkDeviceWaitIdle(vulkan.device);
}

void Engine::onInit()
{

}

void Engine::onCleanup()
{

}

void Engine::onResize(int, int)
{

}

void Engine::draw(VkCommandBuffer)
{

}

void Engine::drawUI()
{

}

void Engine::requestGPUFeatures(GPUFeatures &)
{

}

VkExtent2D Engine::drawExtent()
{
    return {
        static_cast<uint32_t>(swapchain.extent.width * renderScale),
        static_cast<uint32_t>(swapchain.extent.height * renderScale)
    };
}

void Engine::setViewport(VkCommandBuffer cmd, float x, float y, float w, float h, bool invertY)
{
    // negative height to conform to opengl Y up
    VkViewport viewport{ x, y, w, invertY ? -h : h, 0.0f, 1.0f };

    vkCmdSetViewport(cmd, 0, 1, & viewport);
}

void Engine::setScissor(VkCommandBuffer cmd, int x, int y, uint32_t w, uint32_t h)
{
    VkRect2D scissor{ VkOffset2D{ x, y }, VkExtent2D{ w, h } };

    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void Engine::createRenderingResources()
{
    createDrawImage();
    createColorResources();
    createDepthResources();
}

void Engine::cleanupRenderingResources()
{
    vkDestroyImageView(vulkan.device, depthImage.view, nullptr);
    vmaDestroyImage(vulkan.allocator, depthImage.handle, depthImage.allocation);

    vkDestroyImageView(vulkan.device, colorImage.view, nullptr);
    vmaDestroyImage(vulkan.allocator, colorImage.handle, colorImage.allocation);

    vkDestroyImageView(vulkan.device, drawImage.view, nullptr);
    vmaDestroyImage(vulkan.allocator, drawImage.handle, drawImage.allocation);
}

bool Engine::msaaEnabled()
{
    return MSAASamples != VK_SAMPLE_COUNT_1_BIT;
}

void Engine::update(float)
{

}

void Engine::processEvent(const SDL_Event&)
{

}

void Engine::createWindow()
{
	SDL_Init(SDL_INIT_VIDEO);

    uint32_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;

	window = SDL_CreateWindow(
		windowTitle, //window title
		SDL_WINDOWPOS_UNDEFINED, //window position x (don't care)
		SDL_WINDOWPOS_UNDEFINED, //window position y (don't care)
		windowSize.width,  //window width in pixels
		windowSize.height, //window height in pixels
		flags
	);

	if (!window)
		throw std::runtime_error("Failed creating SDL2 window.");

	int width, height;
	SDL_GetWindowSize(window, &width, &height);

	windowSize.width = width;
	windowSize.height = height;
}

void Engine::createVulkan()
{
    requestGPUFeatures(vulkan.requestedFeatures);

    vulkan.requestedFeatures.vk10 = {
        .sampleRateShading = VK_TRUE,
        .fillModeNonSolid = VK_TRUE, // for wireframe rendering
        .samplerAnisotropy = VK_TRUE,
    };

    vulkan.requestedFeatures.vk12 = {
        .descriptorIndexing = VK_TRUE,
        // https://henriquegois.dev/posts/bindless-resources-in-vulkan/
        .descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
    };

    vulkan.requestedFeatures.vk13 = {
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };

    vulkan.create(window);
}

void Engine::createSwapchain()
{
    swapchain.init(vulkan.gpu, vulkan.surface, vulkan.device);
	swapchain.create(windowSize.width, windowSize.height);
}

void Engine::createDrawImage()
{
    auto extent = drawExtent();

    drawImage = createImage(
        VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = swapchain.imageFormat,
            .extent = { extent.width, extent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        },
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        },
        vulkan.allocator
    );

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = drawImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain.imageFormat,
        .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };

    VK_CHECK(vkCreateImageView(vulkan.device, &viewInfo, nullptr, &drawImage.view));
}

void Engine::createColorResources()
{
    auto extent = drawExtent();

    colorImage = createImage(
        VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = swapchain.imageFormat,
            .extent = { extent.width, extent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = MSAASamples,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        },
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        },
        vulkan.allocator
    );

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = colorImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain.imageFormat,
        .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };

    VK_CHECK(vkCreateImageView(vulkan.device, &viewInfo, nullptr, &colorImage.view));
}

void Engine::createDepthResources()
{
    VkFormat format = findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
        vulkan.gpu
    );

    auto extent = drawExtent();

    depthImage = createImage(
        VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = { extent.width, extent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = MSAASamples,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        },
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        },
        vulkan.allocator
    );

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        //VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 }
    };

    VK_CHECK(vkCreateImageView(vulkan.device, &viewInfo, nullptr, &depthImage.view));
}

void Engine::recreateSwapchain()
{
    waitIdle(); // wait for pending operations

    swapchain.create(windowSize.width, windowSize.height);

    cleanupRenderingResources();
    createRenderingResources();

    //vkDestroyRenderPass(vulkan.device, renderPass, nullptr);
    //createDefaultRenderPass();

    // rebuild objects with new swapchain
    //for (auto &frameBuffer : frameBuffers)
    //    vkDestroyFramebuffer(vulkan.device, frameBuffer, nullptr);
    //createFrameBuffers();

    waitIdle(); // wait for pending init
}

void Engine::updateImGui()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    drawUI();

    ImGui::Render();
}

void Engine::drawImGui(VkCommandBuffer cmd)
{
    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain.imageViews[currentImageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        //.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, //VK_ATTACHMENT_STORE_OP_STORE,
    };

    VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .extent = windowSize },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo
    };

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void Engine::createDefaultRenderPass()
{
    VkAttachmentDescription colorAttachment{
        .format = swapchain.imageFormat,
        .samples = MSAASamples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = msaaEnabled() ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentRef{
        // attachment number will index into the pAttachments array in the parent renderpass itself
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentDescription depthAttachment{
        .format = depthImage.info.format,
        .samples = MSAASamples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depthAttachmentRef{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentDescription colorAttachmentResolve{
        .format = swapchain.imageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentResolveRef{
        .attachment = 2,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    // we are going to create 1 subpass, which is the minimum you can do
    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pResolveAttachments = msaaEnabled() ? &colorAttachmentResolveRef : nullptr,
        .pDepthStencilAttachment = &depthAttachmentRef
    };

    VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    };

    std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };

    if (msaaEnabled()) {
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        attachments.push_back(colorAttachmentResolve);
    }

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VK_CHECK(vkCreateRenderPass(vulkan.device, &renderPassInfo, nullptr, &renderPass));
}

void Engine::createFrameBuffers()
{
    // create the framebuffers for the swapchain images.
    // this will connect the render-pass to the images for rendering
    VkFramebufferCreateInfo frameBufferInfo{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .width = swapchain.extent.width,
        .height = swapchain.extent.height,
        .layers = 1
    };

    // grab how many images we have in the swapchain
    const uint32_t imageCount = swapchain.images.size();

    frameBuffers = std::vector<VkFramebuffer>(imageCount);

    // create framebuffers for each of the swapchain image views
    for (uint32_t i = 0; i < imageCount; i++) {
        std::vector<VkImageView> attachments;

        if (msaaEnabled()) {
            attachments = { colorImage.view, depthImage.view, swapchain.imageViews[i] };
        } else {
            attachments = { swapchain.imageViews[i], depthImage.view };
        }

        frameBufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size()),
        frameBufferInfo.pAttachments = attachments.data();

        VK_CHECK(vkCreateFramebuffer(vulkan.device, &frameBufferInfo, nullptr, &frameBuffers[i]));
    }
}

void Engine::createFrameObjects()
{
	framesInFlight.resize(MAX_FRAMES_IN_FLIGHT);

    uint32_t queueFamilyIndex = vulkan.queueFamilies.graphics.value();

    VkCommandPoolCreateInfo commandPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };

    VkSemaphoreCreateInfo semaphoreInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

	for (auto &frame : framesInFlight) {
        VK_CHECK(vkCreateCommandPool(vulkan.device, &commandPoolInfo, nullptr, &frame.commandPool));

        VkCommandBufferAllocateInfo cmdAllocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = frame.commandPool,
            .commandBufferCount = 1
        };

        VK_CHECK(vkAllocateCommandBuffers(vulkan.device, &cmdAllocInfo, &frame.mainCommandBuffer));

        VK_CHECK(vkCreateSemaphore(vulkan.device, &semaphoreInfo, nullptr,
            &frame.imageAvailableSemaphore));

        VK_CHECK(vkCreateFence(vulkan.device, &fenceInfo, nullptr,
			&frame.renderFinishedFence));
	}

    renderFinishedSemaphores.resize(swapchain.images.size());

    for (size_t i = 0; i < swapchain.images.size(); ++i) {
        VkSemaphore renderSemaphore;

        VK_CHECK(vkCreateSemaphore(vulkan.device, &semaphoreInfo, nullptr, &renderSemaphore));

        renderFinishedSemaphores[i] = renderSemaphore;
    }
}

void Engine::createBindlessDescriptors()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, STORAGE_COUNT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SAMPLER_COUNT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, IMAGE_COUNT},
    };

    VkDescriptorPoolCreateInfo poolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VK_CHECK(vkCreateDescriptorPool(vulkan.device, &poolCreateInfo, nullptr, &descriptorPool));

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {STORAGE_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, STORAGE_COUNT, VK_SHADER_STAGE_ALL, nullptr},
        {SAMPLER_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SAMPLER_COUNT, VK_SHADER_STAGE_ALL, nullptr},
        {IMAGE_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, IMAGE_COUNT, VK_SHADER_STAGE_ALL, nullptr}
    };

    std::vector<VkDescriptorBindingFlags> bindingFlags = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindingFlags.size()),
        .pBindingFlags = bindingFlags.data()
    };

    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &setLayoutBindingFlags,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    VK_CHECK(vkCreateDescriptorSetLayout(vulkan.device, &setLayoutCreateInfo, nullptr, &bindlessDescriptorSetLayout));

    std::vector<VkDescriptorSetLayout> sets = { bindlessDescriptorSetLayout };

    VkDescriptorSetAllocateInfo setAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = sets.data()
    };

    VK_CHECK(vkAllocateDescriptorSets(vulkan.device, &setAllocateInfo, &bindlessDescriptorSet));
}

void Engine::createScene()
{
    scene.init(vulkan.device, vulkan.queueFamilies.graphics.value(),
        framesInFlight.size(), vulkan.allocator, bindlessDescriptorSet);
}

void Engine::initImGui()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
         { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
    };

    uint32_t maxSets = 0;
    for (VkDescriptorPoolSize& poolSize : poolSizes)
        maxSets += poolSize.descriptorCount;

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VK_CHECK(vkCreateDescriptorPool(vulkan.device, &poolInfo, nullptr, &imguiDescriptorPool));

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(window);

    //dynamic rendering parameters
    VkPipelineRenderingCreateInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchain.imageFormat,
    };

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = vulkan.instance,
        .PhysicalDevice = vulkan.gpu,
        .Device = vulkan.device,
        .Queue = vulkan.graphicsQueue,
        .DescriptorPool = imguiDescriptorPool,
        .MinImageCount = 2,
        .ImageCount = static_cast<uint32_t>(swapchain.images.size()),
        .PipelineInfoMain = { .MSAASamples = VK_SAMPLE_COUNT_1_BIT, .PipelineRenderingCreateInfo = renderInfo },
        .UseDynamicRendering = true
    };

    ImGui_ImplVulkan_Init(&initInfo);
}

bool Engine::prepareFrame(Frame &frame)
{
	// acquire image from swapchain
    vkWaitForFences(vulkan.device, 1, &frame.renderFinishedFence, VK_TRUE, UINT64_MAX);

	VkResult res = swapchain.acquireNextImage(frame.imageAvailableSemaphore, &currentImageIndex);

    vkResetFences(vulkan.device, 1, &frame.renderFinishedFence);

	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		int width, height;
		SDL_GetWindowSize(window, &width, &height);

		resize(width, height);

		return false;
	} else {
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
			throw std::runtime_error("Failed acquiring image from swapchain.");

		return true;
	}
}

void Engine::presentFrame(Frame &frame)
{
    // Pipeline stages used to wait at for graphics queue submissions
	VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	// submit draw commands in mainbuffer
    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame.mainCommandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphores[currentImageIndex]
    };

    vkResetFences(vulkan.device, 1, &frame.renderFinishedFence);

    VK_CHECK(vkQueueSubmit(vulkan.graphicsQueue, 1, &submit, frame.renderFinishedFence));

	// present frame to swapchain
    VkResult res = swapchain.presentImage(vulkan.presentQueue, currentImageIndex, renderFinishedSemaphores[currentImageIndex]);

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		int width, height;
		SDL_GetWindowSize(window, &width, &height);

		resize(width, height);
	} else {
		if (res != VK_SUCCESS)
			throw std::runtime_error("Failed presenting image to swapchain.");
	}
}
