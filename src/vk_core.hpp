#ifndef VK_CORE_HPP
#define VK_CORE_HPP

#include "vk_mem_alloc.h"

#include <optional>
#include <vector>

class SDL_Window;

namespace vke {

extern int framesInFlightCount;

// https://henriquegois.dev/posts/bindless-resources-in-vulkan/
// Select a binding for each descriptor type
constexpr int STORAGE_BINDING = 0;
constexpr int SAMPLER_BINDING = 1;
constexpr int IMAGE_BINDING   = 2;

// Max count of each descriptor type
// You can query the max values for these with
// physicalDevice.getProperties().limits.maxDescriptrorSet*******
constexpr int STORAGE_COUNT = 65536;
constexpr int SAMPLER_COUNT = 65536;
constexpr int IMAGE_COUNT   = 65536;

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool isComplete() {
        return graphics.has_value() && present.has_value();
    }
};

struct GPUProperties {
    VkPhysicalDeviceProperties vk10;
    VkPhysicalDeviceVulkan11Properties vk11;
    VkPhysicalDeviceVulkan12Properties vk12;
    VkPhysicalDeviceVulkan13Properties vk13;
};

struct GPUFeatures{
    VkPhysicalDeviceFeatures vk10{};
    VkPhysicalDeviceVulkan11Features vk11{};
    VkPhysicalDeviceVulkan12Features vk12{};
    VkPhysicalDeviceVulkan13Features vk13{};
};

struct GPUDetails {
    GPUProperties properties;
    GPUFeatures features;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkSampleCountFlagBits maxMSAASamples;
};

// device queues and properties
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice gpu, VkSurfaceKHR surface);
GPUDetails queryGPUDetails(VkPhysicalDevice gpu);
VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDeviceLimits limits);

// command submission
VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkDevice device);
VkCommandBuffer createCommandBuffer(bool begin, VkCommandPool pool, VkDevice device);
void submitCommandBuffer(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool, VkDevice device, bool free);

class Vulkan {
public:
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice gpu;
    VkDevice device;
    GPUDetails gpuDetails;
    QueueFamilyIndices queueFamilies;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VmaAllocator allocator;

    const char *applicationName = "Vulkan Application";
    uint32_t applicationVersion = VK_MAKE_VERSION(1,0,0);
    const char *engineName = "Vulkan Engine";
    uint32_t engineVersion = VK_MAKE_VERSION(1,0,0);
    uint32_t apiVersion = VK_API_VERSION_1_3;

    GPUFeatures requestedFeatures;
    std::vector<const char*> requestedExtensions;

    void create(SDL_Window *window);
    void cleanup();

protected:
    void createInstance(SDL_Window *window);
    void selectGPU();
    void createDevice();
    void createAllocator();
};

} // end namespace vke

#endif // VK_CORE_HPP
