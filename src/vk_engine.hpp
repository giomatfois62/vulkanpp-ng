#ifndef VULKAN_ENGINE_HPP
#define VULKAN_ENGINE_HPP

#include "vk_core.hpp"
#include "vk_image.hpp"
#include "vk_mesh.hpp"
#include "vk_texture.hpp"

#include <SDL.h>

#include <map>
#include <string>
#include <queue>

namespace vke {

template<typename T>
struct SparseVector {
    SparseVector() { items.resize(1); } // reserve first item

    size_t insert(const T& item) {
        size_t id;

        if (freeIDs.size()) {
            id = freeIDs.front();
            freeIDs.pop();
            items[id] = item;
        } else {
            id = items.size();
            items.push_back(item);
        }

        return id;
    }

    void remove(size_t id) { freeIDs.push(id); }// TODO: cleanup?

    size_t dataSize() { return sizeof(T) * items.size(); }

    T *data() { return items.data(); }

    std::vector<T> items;
    std::queue<size_t> freeIDs;
};

struct Frame {
    VkSemaphore imageAvailableSemaphore;
	VkFence renderFinishedFence;
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;
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

    Texture createTexture(const TextureData &data);
    uint32_t loadTexture(const std::string &path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    uint32_t storeTexture(const Texture &texture);
    uint32_t storeMaterial(const Material &material);
    uint32_t storePBRMaterial(const PBRMaterial &material);

    Model loadModel(std::vector<vke::Vertex> &vertices, const std::vector<uint32_t> &indices);
    Model loadModel(const std::string &path);
    Model loadOBJ(const std::string &path);
    Model loadGLTF(const std::string &path);

    struct {
        SparseVector<Texture> textures;
        std::map<std::string, uint32_t> texturesMap;
        SparseVector<Material> materials;
        std::map<std::string, uint32_t> materialsMap;
        SparseVector<PBRMaterial> pbrMaterials;
        std::map<std::string, uint32_t> pbrMaterialsMap;
    } assets;

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
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout bindlessDescriptorSetLayout;
    VkDescriptorSet bindlessDescriptorSet;

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
    void createBindlessDescriptors();

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
