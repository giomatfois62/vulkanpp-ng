#ifndef VK_RENDERER_HPP
#define VK_RENDERER_HPP

#include "vk_image.hpp"

#include <functional>

namespace vke {

class Renderer
{
public:
    void init(VkDevice device, VkPhysicalDevice gpu, VmaAllocator allocator,
        uint32_t width, uint32_t height, VkFormat imageFormat);
    void resize(uint32_t width, uint32_t height, VkFormat imageFormat);
    void cleanup();
    void render(VkCommandBuffer cmd, VkImage targetImage, std::function<void(VkCommandBuffer)> drawCommands);
    void createResources();
    void setViewport(VkCommandBuffer cmd, float x, float y, float w, float h, bool invertY = true);
    void setScissor(VkCommandBuffer cmd, int x, int y, uint32_t w, uint32_t h);
    void setMSAASamples(VkSampleCountFlagBits samples);

    bool msaaEnabled();
    VkExtent2D drawExtent();

    float renderScale = 1.0f;
    VkSampleCountFlagBits MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    VkSampleCountFlagBits maxMSAASamples = VK_SAMPLE_COUNT_1_BIT;
    VkClearValue clearValue;

    Image drawImage;
    Image colorImage;
    Image depthImage;

protected:
    VkDevice device;
    VkPhysicalDevice gpu;
    VmaAllocator allocator;
    VkExtent2D targetImageExtent;
    VkFormat targetImageFormat;
};

} // end namespace vke

#endif // VK_RENDERER_HPP
