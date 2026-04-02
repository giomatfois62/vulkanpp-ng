#ifndef VULKAN_ENGINE_HPP
#define VULKAN_ENGINE_HPP

#include "vk_core.hpp"
#include "vk_image.hpp"
#include "vk_scene.hpp"

#include <SDL.h>

namespace vke {

struct Frame {
    VkSemaphore imageAvailableSemaphore;
	VkFence renderFinishedFence;
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;
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

    struct {
        double imguiEventsTime;
        double appEventsTime;
        double renderingTime;
    } benchmarks;

protected:
	virtual void onInit();
	virtual void onCleanup();
	virtual void onResize(int w, int h);
	virtual void update(float dt);
    virtual void processEvent(const SDL_Event &event);
	virtual void draw(VkCommandBuffer cmd);
    virtual void drawUI();
    virtual void requestGPUFeatures(GPUFeatures &features);

    VkExtent2D drawExtent();
    void setViewport(VkCommandBuffer cmd, float x, float y, float w, float h, bool invertY = true);
    void setScissor(VkCommandBuffer cmd, int x, int y, uint32_t w, uint32_t h);
    void createRenderingResources();
    void cleanupRenderingResources();

    bool msaaEnabled();
    void recreateSwapchain();
    void waitIdle();

	SDL_Window *window = nullptr;
    VkExtent2D windowSize = { 1024, 768 };
    const char *windowTitle = "Vulkan Application";
    float lagInMillisecs;

    Vulkan vulkan;
	Swapchain swapchain;
    Scene scene;
    Image drawImage;
    Image colorImage;
    Image depthImage;
    float renderScale = 1.0f;
    VkSampleCountFlagBits MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> frameBuffers; // one per swapchain image
    std::vector<Frame> framesInFlight;
    std::vector<VkSemaphore> renderFinishedSemaphores; // one per swapchain image
	uint32_t currentImageIndex = 0;
	uint32_t currentFrameIndex = 0;
	VkClearValue clearValue;
    VkDescriptorPool imguiDescriptorPool;

private:
	void createWindow();
    void createVulkan();
    void createSwapchain();
    void createDrawImage();
    void createColorResources();
    void createDepthResources();
	void createDefaultRenderPass();
    void createFrameBuffers();
    void createFrameObjects();
    void createScene();

    bool prepareFrame(Frame &frame);
    void presentFrame(Frame &frame);
    void renderFrame();
    void beginDraw(VkCommandBuffer cmd);
    void endDraw(VkCommandBuffer cmd);

    void initImGui();
    void updateImGui();
    void drawImGui(VkCommandBuffer cmd);

	bool shouldQuit = false;
    bool isHidden = false;
};

} // end namespace vke

#endif // VULKAN_HPP
