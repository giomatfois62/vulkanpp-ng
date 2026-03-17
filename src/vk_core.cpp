#include "vk_core.hpp"
#include "vk_utils.hpp"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <SDL_video.h>
#include <SDL_vulkan.h>

#include <iostream>
#include <set>

using namespace std;
using namespace vke;

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

QueueFamilyIndices vke::findQueueFamilies(VkPhysicalDevice gpu, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilies.data());

    uint32_t i = 0;

    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics = i;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &presentSupport);

        if (presentSupport)
            indices.present = i;

        if (indices.isComplete())
            break;

        i++;
    }

    return indices;
}

GPUDetails vke::queryGPUDetails(VkPhysicalDevice gpu)
{
    GPUDetails details;

    details.properties.vk13 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES };
    details.properties.vk12 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES, .pNext =  &details.properties.vk13 };
    details.properties.vk11 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES, .pNext =  &details.properties.vk12 };
    VkPhysicalDeviceProperties2 properties2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &details.properties.vk11 };

    details.features.vk13 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    details.features.vk12 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext =  &details.features.vk13 };
    details.features.vk11 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, .pNext =  &details.features.vk12 };
    VkPhysicalDeviceFeatures2 features2 = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &details.features.vk11 };

    vkGetPhysicalDeviceProperties2(gpu, &properties2);
    vkGetPhysicalDeviceFeatures2(gpu, &features2);
    vkGetPhysicalDeviceMemoryProperties(gpu, &details.memoryProperties);

    details.properties.vk10 = properties2.properties;
    details.features.vk10 = features2.features;

    details.maxMSAASamples = getMaxUsableSampleCount(details.properties.vk10.limits);

    return details;
}

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

std::vector<const char*> requiredInstanceExtensions(SDL_Window *window)
{
    uint32_t extensionsCount = 0;

    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionsCount, nullptr))
        throw std::runtime_error("Failed enumerating needed extensions.");

    std::vector<const char*> extensions(extensionsCount);

    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionsCount, extensions.data()))
        throw std::runtime_error("Failed quering needed extensions names.");

    return extensions;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions)
        requiredExtensions.erase(extension.extensionName);

    return requiredExtensions.empty();
}

void Vulkan::create(SDL_Window *window)
{
    createInstance(window);
    selectGPU();
    createDevice();
    createAllocator();
    createCommandPool();
}

void Vulkan::cleanup()
{
    vkDestroyCommandPool(device, commandPool, nullptr);
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
}

VkCommandBuffer Vulkan::createCommandBuffer(bool begin)
{
    VkCommandBufferAllocateInfo cmdAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .commandBufferCount = 1
    };

    VkCommandBuffer cmd;

    VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd));

    if (begin) {
        VkCommandBufferBeginInfo cmdBeginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
    }

    return cmd;
}

void Vulkan::submitCommandBuffer(VkCommandBuffer cmd, VkQueue queue, bool free)
{
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkFence fence;

    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

    VkCommandBufferSubmitInfo cmdSubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd
    };

    VkSubmitInfo2 submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        //.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1,
        //.pWaitSemaphoreInfos = waitSemaphoreInfo,
        //.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1,
        //.pSignalSemaphoreInfos = signalSemaphoreInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdSubmitInfo,
    };

    // submit command buffer to the queue and execute it.
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));
    VK_CHECK(vkQueueSubmit2(queue, 1, &submit, fence));
    VK_CHECK(vkWaitForFences(device, 1, &fence, true, UINT64_MAX));

    vkDestroyFence(device, fence, nullptr);

    if (free) {
        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    }
}

void Vulkan::createInstance(SDL_Window *window)
{
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = applicationName,
        .applicationVersion = applicationVersion,
        .pEngineName = engineName,
        .engineVersion = engineVersion,
        .apiVersion = apiVersion
    };

    // Query extensions needed to run
    std::vector<const char*> extensions = requiredInstanceExtensions(window);

    // Create Instance
    VkInstanceCreateInfo instanceInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance));

    CHECK(SDL_Vulkan_CreateSurface(window, instance, &surface))
}

void Vulkan::selectGPU()
{
    uint32_t gpuCount = 0;
    vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);

    if (gpuCount == 0)
        throw std::runtime_error("No GPU with Vulkan support found.");

    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());

    auto printDevice = [](const VkPhysicalDevice &device) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        cout << "\t" << properties.deviceName <<
        " (Type " << properties.deviceType << ")" << endl;
    };

    cout << "Available Devices:" << endl;
    for (auto &gpu : gpus)
        printDevice(gpu);

    std::vector<VkPhysicalDevice> selectableGPUs;

    for (const auto& device : gpus) {
        QueueFamilyIndices indices = findQueueFamilies(device, surface);
        bool extensionsSupported = checkDeviceExtensionSupport(device);
        bool swapChainAdequate = false;

        if (extensionsSupported) {
            SwapchainSupportDetails swapChainSupport = querySwapchainSupport(device, surface);
            swapChainAdequate = !swapChainSupport.formats.empty() &&
                !swapChainSupport.presentModes.empty();
        }

        if (indices.isComplete() && extensionsSupported && swapChainAdequate)
            selectableGPUs.push_back(device);
    }

    if (selectableGPUs.size() == 0)
        throw std::runtime_error("No GPU with graphics and present queues support found.");

    gpu = selectableGPUs[0];

    // try to find a discrete GPU
    for (const auto& device : selectableGPUs) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            gpu = device;
            break;
        }
    }

    cout << "Selected Device:" << endl;
    printDevice(gpu);

    gpuDetails = queryGPUDetails(gpu);
}

void Vulkan::createDevice()
{
    queueFamilies = findQueueFamilies(gpu, surface);

    std::set<uint32_t> uniqueQueueFamilies = {
        queueFamilies.graphics.value(),
        queueFamilies.present.value()
    };

    float queuePriority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    for (uint32_t index : uniqueQueueFamilies) {
        queueInfos.push_back(
            VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = index,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority
            }
        );
    }

    requestedFeatures.vk13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    requestedFeatures.vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    requestedFeatures.vk11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    requestedFeatures.vk12.pNext = &requestedFeatures.vk13;
    requestedFeatures.vk11.pNext = &requestedFeatures.vk12;

    VkDeviceCreateInfo deviceInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &requestedFeatures.vk11,
        .queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()),
        .pQueueCreateInfos = queueInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &requestedFeatures.vk10,
    };

    VK_CHECK(vkCreateDevice(gpu, &deviceInfo, nullptr, &device));

    vkGetDeviceQueue(device, queueFamilies.graphics.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, queueFamilies.present.value(), 0, &presentQueue);
}

void Vulkan::createAllocator()
{
    VmaVulkanFunctions vkFunctions{
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkCreateImage = vkCreateImage
    };

    VmaAllocatorCreateInfo createInfo{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = gpu,
        .device = device,
        .pVulkanFunctions = &vkFunctions,
        .instance = instance,
        .vulkanApiVersion = apiVersion
    };

    VK_CHECK(vmaCreateAllocator(&createInfo, &allocator));
}

void Vulkan::createCommandPool()
{
    commandPool = vke::createCommandPool(queueFamilies.graphics.value(), device);
}

void Swapchain::init(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device)
{
    this->gpu = physicalDevice;
    this->surface = surface;
    this->device = device;
}

void Swapchain::create(uint32_t width, uint32_t height)
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
    uint32_t imageCount = minCount + 1;

    if (maxCount > 0 && imageCount > maxCount)
        imageCount = maxCount;

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

    auto queueFamilyIndices = findQueueFamilies(gpu, surface);

    if (queueFamilyIndices.graphics != queueFamilyIndices.present) {
        uint32_t allIndices[]{
            queueFamilyIndices.graphics.value(),
            queueFamilyIndices.present.value()
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

VkSampleCountFlagBits vke::getMaxUsableSampleCount(VkPhysicalDeviceLimits limits)
{
    VkSampleCountFlags counts = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

VkCommandBuffer vke::createCommandBuffer(bool begin, VkCommandPool pool, VkDevice device)
{
    VkCommandBufferAllocateInfo cmdAllocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .commandBufferCount = 1
    };

    VkCommandBuffer cmd;

    VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd));

    if (begin) {
        VkCommandBufferBeginInfo cmdBeginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
    }

    return cmd;
}

void vke::submitCommandBuffer(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool, VkDevice device, bool free)
{
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkFence fence;

    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

    VkCommandBufferSubmitInfo cmdSubmitInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmd
    };

    VkSubmitInfo2 submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        //.waitSemaphoreInfoCount = waitSemaphoreInfo == nullptr ? 0 : 1,
        //.pWaitSemaphoreInfos = waitSemaphoreInfo,
        //.signalSemaphoreInfoCount = signalSemaphoreInfo == nullptr ? 0 : 1,
        //.pSignalSemaphoreInfos = signalSemaphoreInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdSubmitInfo,
    };

    // submit command buffer to the queue and execute it.
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &fence));
    VK_CHECK(vkQueueSubmit2(queue, 1, &submit, fence));
    VK_CHECK(vkWaitForFences(device, 1, &fence, true, UINT64_MAX));

    vkDestroyFence(device, fence, nullptr);

    if (free) {
        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }
}

VkCommandPool vke::createCommandPool(uint32_t queueFamilyIndex, VkDevice device)
{
    VkCommandPoolCreateInfo commandPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };

    VkCommandPool commandPool;

    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool));

    return commandPool;
}
