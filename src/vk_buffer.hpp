#ifndef VK_BUFFER_HPP
#define VK_BUFFER_HPP

#include <vulkan/vulkan.h>

#include <vector>

#include "vk_mem_alloc.h"

namespace vke {

struct Buffer {
    VkBuffer handle;
    VkBufferCreateInfo info;
    VmaAllocation allocation;
    VmaAllocationInfo allocInfo;
    VkDeviceAddress address;
};

struct ShaderBuffers {
    std::vector<Buffer> buffers;

    void createUniform(uint32_t count, size_t size, VkDevice device, VmaAllocator allocator);
    void createStorage(uint32_t count, size_t size, VkDevice device, VmaAllocator allocator);
    void update(uint32_t index, void *data, size_t size, size_t offset = 0);
    void cleanup(VmaAllocator allocator);
};

Buffer createBuffer(VkBufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator);

Buffer createStagingBuffer(size_t size, VmaAllocator allocator);

Buffer createUBO(size_t size, VmaAllocator allocator);

Buffer createSSBO(size_t size, VmaAllocator allocator);

VkDeviceAddress getBufferAddress(VkBuffer buffer, VkDevice device);

} // end namespace vke

#endif // VK_BUFFER_HPP
