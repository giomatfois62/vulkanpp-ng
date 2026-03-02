#ifndef VK_CORE_HPP
#define VK_CORE_HPP

#include "vk_mem_alloc.h"

#include <optional>
#include <vector>

class SDL_Window;

namespace vke {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool isComplete() {
        return graphics.has_value() && present.has_value();
    }
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice gpu, VkSurfaceKHR surface);

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

GPUDetails queryGPUDetails(VkPhysicalDevice gpu);
VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDeviceLimits limits);

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface);

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
    VkCommandPool commandPool;

    const char *applicationName = "Vulkan Application";
    uint32_t applicationVersion = VK_MAKE_VERSION(1,0,0);
    const char *engineName = "Vulkan Engine";
    uint32_t engineVersion = VK_MAKE_VERSION(1,0,0);
    uint32_t apiVersion = VK_API_VERSION_1_3;

    GPUFeatures requestedFeatures;
    std::vector<const char*> requestedExtensions;

    void create(SDL_Window *window);
    void cleanup();

    VkCommandBuffer createCommandBuffer(bool begin);
    void submitCommandBuffer(VkCommandBuffer cmd, VkQueue queue, bool free);

protected:
    void createInstance(SDL_Window *window);
    void selectGPU();
    void createDevice();
    void createAllocator();
    void createCommandPool();
};

class Swapchain {
public:
    void init(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device);
    void create(uint32_t width, uint32_t height);
    void cleanup();

    VkResult acquireNextImage(VkSemaphore signalSemaphore, uint32_t* imageIndex);
    VkResult presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat imageFormat;
    VkSurfaceFormatKHR surfaceFormat = { .format = VK_FORMAT_B8G8R8A8_SRGB, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    VkExtent2D extent;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    SwapchainSupportDetails supportDetails;

protected:
    VkPhysicalDevice gpu;
    VkSurfaceKHR surface;
    VkDevice device;
};

} // end namespace vke

#endif // VK_CORE_HPP
