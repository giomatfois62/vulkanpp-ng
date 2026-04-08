#ifndef VULKAN_ENGINE_HPP
#define VULKAN_ENGINE_HPP

#include "vk_core.hpp"
#include "vk_image.hpp"
#include "vk_renderer.hpp"
#include "vk_scene.hpp"
#include "vk_ui.hpp"

#include <SDL.h>

namespace vke {

struct Window {
    SDL_Window *handle = nullptr;
    VkExtent2D extent = { 1024, 768 };
    const char *title = "Vulkan Application";

    void cleanup();
};

struct Frame {
    VkSemaphore imageAvailableSemaphore;
	VkFence renderFinishedFence;
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;

    void cleanup(VkDevice device);
};

struct BindlessPushConstants {
    VkDeviceAddress camera;
    VkDeviceAddress instances;
    VkDeviceAddress materials;
    VkDeviceAddress lights;
};

class Engine {
public:
	Engine(int argc, char**argv);

	void run();
	void quit();

	void setWindowTitle(const char* title);
	void setApplicationName(const char *name);

	void init();
	void loop();
	void cleanup();
	void processEvents();
    void resize(int width, int height);

protected:
	virtual void onInit();
	virtual void onCleanup();
	virtual void onResize(int w, int h);
	virtual void update(float dt);
    virtual void processEvent(const SDL_Event &event);
	virtual void draw(VkCommandBuffer cmd);
    virtual void drawUI();
    virtual void requestGPUFeatures(GPUFeatures &features);

    void waitIdle();

	SDL_Window *window = nullptr;
    VkExtent2D windowSize = { 1024, 768 };
    const char *windowTitle = "Vulkan Application";
    float lagInMillisecs;

    Vulkan vulkan;
	Swapchain swapchain;
    Scene scene;
    Renderer renderer;
    UI ui;

    std::vector<Frame> framesInFlight;
    std::vector<VkSemaphore> renderFinishedSemaphores; // one per swapchain image
	uint32_t currentImageIndex = 0;
	uint32_t currentFrameIndex = 0;

private:
	void createWindow();
    void createVulkan();
    void createSwapchain();
    void createFrames();
    void createScene();
    void createRenderer();
    void createUI();

    bool prepareFrame(Frame &frame);
    void presentFrame(Frame &frame);
    void renderFrame();

	bool shouldQuit = false;
    bool isHidden = false;
};

} // end namespace vke

#endif // VULKAN_HPP
