#ifndef VK_TEXTURE_HPP
#define VK_TEXTURE_HPP

#include "vk_image.hpp"

#include <string>

namespace vke {

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
    VkDevice device;
    VmaAllocator allocator;

    void create(const TextureData &data, VkCommandPool pool, VkQueue queue, VkDevice device, VmaAllocator allocator);
    void cleanup(VkDevice device, VmaAllocator allocator);
};

TextureData loadTextureDataStb(const std::string &path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

TextureData loadTextureDataKtx(const std::string &path);

TextureData loadTextureDataKtx(const uint8_t* data, size_t size);

TextureData loadTextureData(const std::string &path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

uint32_t computeMipLevels(uint32_t width, uint32_t height);

void generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize, uint32_t mipLevels, VkFilter filter = VK_FILTER_LINEAR);

VkSampler createDefaultSampler(VkDevice device);

} // end namespace vke

#endif // VK_TEXTURE_HPP
