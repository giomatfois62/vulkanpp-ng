#include "vk_renderer.hpp"
#include "vk_core.hpp"
#include "vk_utils.hpp"

#include <iostream>

using namespace vke;

void Renderer::init(VkDevice device, VkPhysicalDevice gpu, VmaAllocator allocator, uint32_t width, uint32_t height, VkFormat imageFormat)
{
    this->device = device;
    this->gpu = gpu;
    this->allocator = allocator;
    this->targetImageExtent = { width, height };
    this->targetImageFormat = imageFormat;

    GPUDetails gpuDetails = queryGPUDetails(gpu);
    maxMSAASamples = getMaxUsableSampleCount(gpuDetails.properties.vk10.limits);

    createResources();
}

void Renderer::resize(uint32_t width, uint32_t height, VkFormat imageFormat)
{
    this->targetImageExtent = { width, height };
    this->targetImageFormat = imageFormat;

    cleanup();
    createResources();
}

void Renderer::cleanup()
{
    vkDestroyImageView(device, depthImage.view, nullptr);
    vmaDestroyImage(allocator, depthImage.handle, depthImage.allocation);

    vkDestroyImageView(device, colorImage.view, nullptr);
    vmaDestroyImage(allocator, colorImage.handle, colorImage.allocation);

    vkDestroyImageView(device, drawImage.view, nullptr);
    vmaDestroyImage(allocator, drawImage.handle, drawImage.allocation);
}

void Renderer::render(VkCommandBuffer cmd, VkImage targetImage, std::function<void (VkCommandBuffer)> drawCommands)
{
    // transition images
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

    // begin rendering
    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = msaaEnabled() ? colorImage.view : drawImage.view,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = msaaEnabled() ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE,
        .resolveImageView = msaaEnabled() ? drawImage.view : VK_NULL_HANDLE,
        .resolveImageLayout = msaaEnabled() ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        // on nvidia ".storeOp = OP_DONT_CARE" doesn't work without MSAA
        .storeOp = msaaEnabled() ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearValue
    };

    VkRenderingAttachmentInfo depthAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depthImage.view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = VkClearValue{ .depthStencil{ 1.0f, 0 } }
    };

    auto extent = drawExtent();

    // set pipeline dynamic states
    setViewport(cmd, 0.0f, extent.height, extent.width, extent.height);
    setScissor(cmd, 0, 0, extent.width, extent.height);

    // begin rendering
    VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .extent = extent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo,
        .pDepthAttachment = &depthAttachmentInfo
    };

    vkCmdBeginRendering(cmd, &renderInfo);

    // execute draw commands
    drawCommands(cmd);

    // end rendering
    vkCmdEndRendering(cmd);

    // copy drawImage to targetImage
    changeImageLayout(cmd, drawImage.handle,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );

    changeImageLayout(cmd, targetImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // if rendering offscreen
        //VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // if rendering or resolving directly on it
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );

    copyImageToImage(cmd, drawImage.handle, targetImage, extent, targetImageExtent, VK_FILTER_LINEAR);

    changeImageLayout(cmd, targetImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );
}

void Renderer::createResources()
{
    // create drawImage
    auto extent = drawExtent();

    drawImage = createImage(
        VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = targetImageFormat,
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
        allocator
    );

    VkImageViewCreateInfo drawImageViewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = drawImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = targetImageFormat,
        .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };

    VK_CHECK(vkCreateImageView(device, &drawImageViewInfo, nullptr, &drawImage.view));

    // create colorImage
    colorImage = createImage(
        VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = targetImageFormat,
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
        allocator
    );

    VkImageViewCreateInfo colorImageViewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = colorImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = targetImageFormat,
        .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };

    VK_CHECK(vkCreateImageView(device, &colorImageViewInfo, nullptr, &colorImage.view));

    // create depthImage
    VkFormat depthFormat = findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
        gpu
    );

    depthImage = createImage(
        VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = depthFormat,
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
        allocator
    );

    VkImageViewCreateInfo depthImageViewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depthFormat,
        //VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 }
    };

    VK_CHECK(vkCreateImageView(device, &depthImageViewInfo, nullptr, &depthImage.view));
}

VkExtent2D Renderer::drawExtent()
{
    return {
        static_cast<uint32_t>(targetImageExtent.width * renderScale),
        static_cast<uint32_t>(targetImageExtent.height * renderScale)
    };
}

void Renderer::setViewport(VkCommandBuffer cmd, float x, float y, float w, float h, bool invertY)
{
    // negative height to conform to opengl Y up
    VkViewport viewport{ x, y, w, invertY ? -h : h, 0.0f, 1.0f };

    vkCmdSetViewport(cmd, 0, 1, & viewport);
}

void Renderer::setScissor(VkCommandBuffer cmd, int x, int y, uint32_t w, uint32_t h)
{
    VkRect2D scissor{ VkOffset2D{ x, y }, VkExtent2D{ w, h } };

    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

bool Renderer::msaaEnabled()
{
    return MSAASamples != VK_SAMPLE_COUNT_1_BIT;
}

void Renderer::setMSAASamples(VkSampleCountFlagBits samples)
{
    if (samples > maxMSAASamples) {
        std::cerr << "Unable to set " << samples << " MSAA samples, not supported" << std::endl;
        return;
    }

    MSAASamples = samples;

    cleanup();
    createResources();
}
