#include "vk_engine.hpp"
#include "vk_utils.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"

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

void Window::cleanup()
{
    SDL_DestroyWindow(handle);
    handle = nullptr;
}

void Frame::cleanup(VkDevice device)
{
    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
    vkDestroyFence(device, renderFinishedFence, nullptr);

    vkFreeCommandBuffers(device, commandPool, 1, &mainCommandBuffer);
    vkDestroyCommandPool(device, commandPool, nullptr);
}

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
    createFrames();
    createScene();
    createRenderer();
    createUI();

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

    ui.cleanup();
    renderer.cleanup();
    scene.cleanup();

    for (auto &frame : framesInFlight)
        frame.cleanup(vulkan.device);

    for (auto &semaphore: renderFinishedSemaphores)
        vkDestroySemaphore(vulkan.device, semaphore, nullptr);

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

        ui.processEvent(e);

        // return if io.WantCaptureMouse or io.WantCaptureKeyboard is true
        if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && ImGui::GetIO().WantCaptureKeyboard)
            return;

        processEvent(e);
	}
}

void Engine::resize(int width, int height)
{
	windowSize.width = width;
	windowSize.height = height;

    waitIdle(); // wait for pending operations

    swapchain.resize(width, height);
    renderer.resize(width, height, swapchain.imageFormat);
    ui.resize(width, height);

    waitIdle(); // wait for pending init

	onResize(width, height);
}

void Engine::renderFrame()
{
    ui.update([&]{ drawUI(); });

    Frame currentFrame = framesInFlight[currentFrameIndex];

	if (!prepareFrame(currentFrame))
		return;

    VkCommandBuffer cmd = currentFrame.mainCommandBuffer;

    // begin command buffer
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo cmdBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // render scene
    renderer.render(cmd, swapchain.images[currentImageIndex], [&](VkCommandBuffer cmd){ draw(cmd); });

    // render ui
    ui.render(cmd, swapchain.imageViews[currentImageIndex]);

    // end command buffer
    changeImageLayout(cmd, swapchain.images[currentImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );

    VK_CHECK(vkEndCommandBuffer(cmd));

    // present frame
	presentFrame(currentFrame);

    currentFrameIndex = (currentFrameIndex + 1) % framesInFlightCount;
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
        .multiDrawIndirect = VK_TRUE,
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
    swapchain.resize(windowSize.width, windowSize.height);
}

void Engine::createFrames()
{
    framesInFlight.resize(framesInFlightCount);

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

void Engine::createScene()
{
    scene.init(vulkan.device, vulkan.queueFamilies.graphics.value(), vulkan.allocator);
}

void Engine::createRenderer()
{
    renderer.init(vulkan.device, vulkan.gpu, vulkan.allocator, windowSize.width, windowSize.height, swapchain.imageFormat);
}

void Engine::createUI()
{
    ui.init(vulkan.instance, vulkan.device, vulkan.gpu, vulkan.graphicsQueue, window, swapchain.imageFormat, swapchain.images.size());
    ui.resize(windowSize.width, windowSize.height);
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
