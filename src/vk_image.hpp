#ifndef VK_IMAGE_H
#define VK_IMAGE_H

#include <vulkan/vulkan.h>
#include <vector>

#include "vk_mem_alloc.h"

namespace vke {

struct Image {
    VkImage handle;
    VkImageView view;
    VkImageCreateInfo info;
    VmaAllocation allocation;
};

VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features, VkPhysicalDevice physicalDevice);

VkAccessFlags getAccessFlags(VkImageLayout layout);

VkPipelineStageFlags getPipelineStageFlags(VkImageLayout layout);

Image createImage(VkImageCreateInfo imageInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator);

void changeImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange range);

void copyImageToImage(VkCommandBuffer cmd, VkImage src, VkImage dst, VkExtent2D srcSize, VkExtent2D dstSize, VkFilter filter);

} // end namespace vke

#endif // VK_IMAGE_H
