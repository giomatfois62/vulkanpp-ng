#ifndef VK_IMAGE_H
#define VK_IMAGE_H

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#include "vk_mem_alloc.h"

namespace vke {

struct Image {
    VkImage handle;
    VkImageView view;
    VkImageCreateInfo info;
    VmaAllocation allocation;
};

struct TextureData {
    std::vector<uint8_t> pixels;
    size_t imageSize;
    VkFormat format;
    VkExtent3D extent;
    uint32_t mipLevels = 1;
    uint32_t arrayLayers = 1;
    std::vector<std::vector<size_t>> offsets;
    bool cubeMap = false;
    bool hasMipmaps = false;
};

struct Texture {
    Image image;
    VkSampler sampler;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;

    void cleanup();
};

VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features, VkPhysicalDevice physicalDevice);

VkAccessFlags getAccessFlags(VkImageLayout layout);
VkPipelineStageFlags getPipelineStageFlags(VkImageLayout layout);

Image createImage(VkImageCreateInfo imageInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator);

void changeImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageSubresourceRange range);
void copyImageToImage(VkCommandBuffer cmd, VkImage src, VkImage dst, VkExtent2D srcSize, VkExtent2D dstSize, VkFilter filter);

TextureData loadTextureDataStb(const std::string &path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
TextureData loadTextureDataKtx(const std::string &path);
TextureData loadTextureDataKtx(const uint8_t* data, size_t size);
TextureData loadTextureData(const std::string &path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

uint32_t computeMipLevels(uint32_t width, uint32_t height);
void generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize, uint32_t mipLevels, VkFilter filter = VK_FILTER_LINEAR);

VkSampler createDefaultSampler(VkDevice device);

} // end namespace vke

#endif // VK_IMAGE_H
