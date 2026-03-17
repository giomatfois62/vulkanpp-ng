#include "vk_texture.hpp"
#include "vk_core.hpp"
#include "vk_utils.hpp"
#include "vk_buffer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <ktx.h>
#include <ktxvulkan.h>

#include <cstring>
#include <cmath>

using namespace vke;

void Texture::create(const TextureData &data, VkCommandPool pool, VkQueue queue, VkDevice device, VmaAllocator allocator)
{
    this->device = device;
    this->allocator = allocator;

    Buffer uploadbuffer = createStagingBuffer(data.imageSize, allocator);
    memcpy(uploadbuffer.allocInfo.pMappedData, data.pixels.data(), data.imageSize);

    image = createImage(
        VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = data.format,
            .extent = data.extent,
            .mipLevels = data.mipLevels,
            .arrayLayers = data.arrayLayers,
            .samples = VK_SAMPLE_COUNT_1_BIT, // TODO: support multisampling
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        },
        VmaAllocationCreateInfo{ .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE },
        allocator
    );

    VkImageSubresourceRange subresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = data.mipLevels,
        .layerCount = data.arrayLayers
    };

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D, // TODO: support cubemaps and arrays
        .format = data.format,
        .subresourceRange = subresourceRange
    };

    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &image.view));

    VkCommandBuffer cmd = createCommandBuffer(true, pool, device);

    changeImageLayout(cmd, image.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange
    );

    if (!data.hasMipmaps) {
        VkBufferImageCopy copyRegion{
            .imageSubresource = VkImageSubresourceLayers{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = data.arrayLayers
            },
            .imageExtent = data.extent
        };

        vkCmdCopyBufferToImage(cmd, uploadbuffer.handle, image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        // TODO: support layers mipmap generation
        generateMipmaps(cmd, image.handle, {data.extent.width, data.extent.height}, data.mipLevels);
    } else {
        std::vector<VkBufferImageCopy> copyRegions;

        for (uint32_t layer = 0; layer < data.arrayLayers; ++layer) {
            for (uint32_t mip = 0; mip < data.mipLevels; ++mip) {
                size_t mipOffset = data.offsets[layer][mip];

                copyRegions.push_back(
                    VkBufferImageCopy{
                        .bufferOffset = mipOffset,
                        .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = mip, .baseArrayLayer = layer, .layerCount = 1 },
                        .imageExtent{ .width = data.extent.width >> mip, .height = data.extent.height >> mip, .depth = 1 },
                    }
                );
            }
        }

        vkCmdCopyBufferToImage(cmd, uploadbuffer.handle, image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

        changeImageLayout(cmd, image.handle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresourceRange
        );
    }

    submitCommandBuffer(cmd, queue, pool, device, true);

    vmaDestroyBuffer(allocator, uploadbuffer.handle, uploadbuffer.allocation);

    sampler = createDefaultSampler(device);
}

void Texture::cleanup(VkDevice device, VmaAllocator allocator)
{
    vkDestroySampler(device, sampler, nullptr);
    vkDestroyImageView(device, image.view, nullptr);
    vmaDestroyImage(allocator, image.handle, image.allocation);
}

uint32_t vke::computeMipLevels(uint32_t width, uint32_t height)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

void vke::generateMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize, uint32_t mipLevels, VkFilter filter)
{
    for (uint32_t mip = 0; mip < mipLevels; ++mip) {
        VkExtent2D halfSize = { imageSize.width / 2, imageSize.height / 2 };

        changeImageLayout(cmd, image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = mip, .levelCount = 1, .layerCount = 1 }
        );

        if (mip < mipLevels - 1) {
            VkImageBlit2 blitRegion{
                .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                .pNext = nullptr,
                .srcSubresource = VkImageSubresourceLayers{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = mip,
                    .layerCount = 1
                },
                .srcOffsets = {{ 0, 0, 0 }, {static_cast<int32_t>(imageSize.width), static_cast<int32_t>(imageSize.height), 1}},
                .dstSubresource = VkImageSubresourceLayers{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = mip + 1,
                    .layerCount = 1
                },
                .dstOffsets = {{ 0, 0, 0 }, {static_cast<int32_t>(halfSize.width), static_cast<int32_t>(halfSize.height), 1}},
            };

            VkBlitImageInfo2 blitInfo{
                .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                .srcImage = image,
                .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .dstImage = image,
                .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .regionCount = 1,
                .pRegions = &blitRegion,
                .filter = filter
            };

            vkCmdBlitImage2(cmd, &blitInfo);

            imageSize = halfSize;
        }
    }

    // transition all mip levels into the final read_only layout
    changeImageLayout(cmd, image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = mipLevels, .layerCount = 1 }
    );
}

VkSampler vke::createDefaultSampler(VkDevice device)
{
    // TODO: add anisotropy
    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .maxAnisotropy = 1.0f,
        //.minLod = static_cast<float>(texture.mipLevels / 2), // test mipLevels
        .maxLod = VK_LOD_CLAMP_NONE
    };

    VkSampler sampler;

    VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));

    return sampler;
}

TextureData vke::loadTextureData(const std::string &path, VkFormat format)
{
    std::string extension = getFileExtension(path);

    if (extension == "ktx" || extension == "ktx2") {
        return loadTextureDataKtx(path);
    } else if (extension == "jpg" || extension == "jpeg" || extension == "png") {
        return loadTextureDataStb(path, format);
    } else {
        return loadTextureDataStb(path, format); // generic fallback
    }
}

TextureData vke::loadTextureDataStb(const std::string &path, VkFormat format)
{
    int texWidth = 0, texHeight = 0, texChannels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    size_t imageSize = texWidth * texHeight * 4;

    if (!pixels){
        throw std::runtime_error("Failed loading image data from " + path);
    }

    TextureData data = {
        .pixels = { pixels, pixels + imageSize },
        .imageSize = imageSize,
        //.format = VK_FORMAT_R8G8B8A8_UNORM,
        .format = format,
        .extent = { .width = static_cast<uint32_t>(texWidth), .height = static_cast<uint32_t>(texHeight), .depth = 1 },
        .mipLevels = computeMipLevels(texWidth, texHeight),
        .arrayLayers = 1
    };

    stbi_image_free(pixels);

    return data;
}

TextureData vke::loadTextureDataKtx(const std::string &path)
{
    ktxTexture* ktxTexture = nullptr;
    ktxTexture_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);

    if (!ktxTexture){
        throw std::runtime_error("Failed loading ktx data from " + path);
    }

    TextureData data = {
        .pixels = { ktxTexture->pData, ktxTexture->pData + ktxTexture->dataSize },
        .imageSize = ktxTexture->dataSize,
        .format = ktxTexture_GetVkFormat(ktxTexture), // TODO: force to srgb if ktx-1
        .extent = { .width = ktxTexture->baseWidth, .height = ktxTexture->baseWidth, .depth = 1 },
        .mipLevels = ktxTexture->numLevels,
        .arrayLayers = ktxTexture->numLayers,
        .hasMipmaps = ktxTexture->numLevels > 1
    };

    data.offsets.resize(data.arrayLayers);

    for (uint32_t layer = 0; layer < data.arrayLayers; ++layer) {
        for (uint32_t mip = 0; mip < data.mipLevels; ++mip) {
            size_t mipOffset = 0;
            ktxTexture_GetImageOffset(ktxTexture, mip, layer, 0, &mipOffset);
            data.offsets[layer].push_back(mipOffset);
        }
    }

    ktxTexture_Destroy(ktxTexture);

    return data;
}

TextureData vke::loadTextureDataKtx(const uint8_t *texData, size_t texSize)
{
    ktxTexture* ktxTexture = nullptr;
    ktxTexture_CreateFromMemory(texData, texSize, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);

    if (!ktxTexture){
        throw std::runtime_error("Failed loading ktx data from memory");
    }

    TextureData data = {
        .pixels = { ktxTexture->pData, ktxTexture->pData + ktxTexture->dataSize },
        .imageSize = ktxTexture->dataSize,
        .format = ktxTexture_GetVkFormat(ktxTexture), // TODO: force to srgb if ktx-1
        .extent = { .width = ktxTexture->baseWidth, .height = ktxTexture->baseWidth, .depth = 1 },
        .mipLevels = ktxTexture->numLevels,
        .arrayLayers = ktxTexture->numLayers,
        .hasMipmaps = ktxTexture->numLevels > 1
    };

    data.offsets.resize(data.arrayLayers);

    for (uint32_t layer = 0; layer < data.arrayLayers; ++layer) {
        for (uint32_t mip = 0; mip < data.mipLevels; ++mip) {
            size_t mipOffset = 0;
            ktxTexture_GetImageOffset(ktxTexture, mip, layer, 0, &mipOffset);
            data.offsets[layer].push_back(mipOffset);
        }
    }

    ktxTexture_Destroy(ktxTexture);

    return data;
}
