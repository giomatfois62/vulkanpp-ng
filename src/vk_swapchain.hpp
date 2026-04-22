#ifndef VK_SWAPCHAIN_HPP
#define VK_SWAPCHAIN_HPP

#include <vulkan/vulkan.h>

#include <vector>

namespace vke {

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface);

class Swapchain {
public:
    void init(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device, uint32_t graphicsQueueIndex, uint32_t presentQueueIndex);
    void resize(uint32_t width, uint32_t height);
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
    uint32_t graphicsQueueIndex;
    uint32_t presentQueueIndex;
};

} // end namespace vke

#endif // VK_SWAPCHAIN_HPP
