#include "vk_swapchain.hpp"
#include "vk_utils.hpp"

using namespace vke;

SwapchainSupportDetails vke::querySwapchainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface)
{
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void Swapchain::init(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device, uint32_t graphicsQueueIndex, uint32_t presentQueueIndex)
{
    this->gpu = physicalDevice;
    this->surface = surface;
    this->device = device;
    this->graphicsQueueIndex = graphicsQueueIndex;
    this->presentQueueIndex = presentQueueIndex;
}

void Swapchain::resize(uint32_t width, uint32_t height)
{
    VkSwapchainKHR oldSwapchain = swapchain;

    auto chooseSwapSurfaceFormat = [&](const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == surfaceFormat.format &&
                availableFormat.colorSpace == surfaceFormat.colorSpace)
                return availableFormat;
        }

        return availableFormats[0]; // risky choice
    };

    auto chooseSwapPresentMode = [&](const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == presentMode)
                return availablePresentMode;
        }

        return VK_PRESENT_MODE_FIFO_KHR; // always supported as per spec.
    };

    auto chooseSwapExtent = [&](const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            return capabilities.currentExtent;
        } else {
            VkExtent2D actualExtent{ width, height };

            actualExtent.width = std::max(capabilities.minImageExtent.width,
                                          std::min(capabilities.maxImageExtent.width, actualExtent.width));

            actualExtent.height = std::max(capabilities.minImageExtent.height,
                                           std::min(capabilities.maxImageExtent.height, actualExtent.height));

            return actualExtent;
        }
    };

    // refresh swapchain support details!!!
    supportDetails = querySwapchainSupport(gpu, surface);

    surfaceFormat = chooseSwapSurfaceFormat(supportDetails.formats);
    imageFormat = surfaceFormat.format;
    presentMode = chooseSwapPresentMode(supportDetails.presentModes);
    extent = chooseSwapExtent(supportDetails.capabilities);

    // request one more image to the swapchain
    uint32_t minCount = supportDetails.capabilities.minImageCount;
    uint32_t maxCount = supportDetails.capabilities.maxImageCount;
    uint32_t imageCount = (maxCount > 0) ? std::min(minCount + 1, maxCount) : minCount + 1;

    VkSwapchainCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1, // > 1 for stereoscopic 3D application
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, // VK_IMAGE_USAGE_TRANSFER_DST_BIT is for offscreen rendering
        .preTransform = supportDetails.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // alpha can be used for blending
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = oldSwapchain // set this if swapchain is obsolete (e.g. on window resize)
    };

    if (graphicsQueueIndex != presentQueueIndex) {
        uint32_t allIndices[]{
            graphicsQueueIndex,
            presentQueueIndex
        };

        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = allIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain));

    if (oldSwapchain != VK_NULL_HANDLE) {
        // cleanup old swapchain
        for (size_t i = 0; i < imageViews.size(); i++)
            vkDestroyImageView(device, imageViews[i], nullptr);

        vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
    }

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);

    images.resize(imageCount);

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

    imageViews.resize(imageCount);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = imageFormat,
        .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };

    for (size_t i = 0; i < imageCount; i++) {
        viewInfo.image = images[i];
        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &imageViews[i]));
    }
}

void Swapchain::cleanup()
{
    for (size_t i = 0; i < imageViews.size(); i++)
        vkDestroyImageView(device, imageViews[i], nullptr);

    vkDestroySwapchainKHR(device, swapchain, nullptr);
}

VkResult Swapchain::acquireNextImage(VkSemaphore signalSemaphore, uint32_t *imageIndex)
{
    return vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, signalSemaphore, nullptr, imageIndex);
}

VkResult Swapchain::presentImage(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore)
{
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex
    };

    // Check if a wait semaphore has been specified to wait for before presenting the image
    if (waitSemaphore != VK_NULL_HANDLE) {
        presentInfo.pWaitSemaphores = &waitSemaphore;
        presentInfo.waitSemaphoreCount = 1;
    }

    return vkQueuePresentKHR(queue, &presentInfo);
}
