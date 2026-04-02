#ifndef VK_BUFFER_HPP
#define VK_BUFFER_HPP

#include <vulkan/vulkan.h>

#include <vector>

#include "vk_mem_alloc.h"

namespace vke {

enum class BufferType : uint32_t {
    Uniform = 0,
    Storage = 1,
    Vertex = 2,
    Indirect = 3
};

struct Buffer {
    VkBuffer handle;
    VkBufferCreateInfo info;
    VmaAllocation allocation;
    VmaAllocationInfo allocInfo;
    VkDeviceAddress address;
};

struct ShaderBuffers {
    std::vector<Buffer> buffers;
    size_t size = 0;
    BufferType type = BufferType::Uniform;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;

    const VkBuffer &buffer(uint32_t index);
    const VkDeviceAddress &deviceAddress(uint32_t index);
    void update(uint32_t index, const void *data, size_t size, size_t offset = 0);
    void create(uint32_t count, size_t size, VkDevice device, VmaAllocator allocator);
    void recreate(uint32_t count, size_t size);
    void cleanup();

protected:
    ShaderBuffers() {}
};

struct UBOs : public ShaderBuffers {
    UBOs() { type = BufferType::Uniform; }
};

struct SSBOs : public ShaderBuffers {
    SSBOs() { type = BufferType::Storage; }
};

struct VBOs : public ShaderBuffers {
    VBOs() { type = BufferType::Vertex; }
};

struct IndirectBuffers : public ShaderBuffers {
    IndirectBuffers() { type = BufferType::Indirect; }
};

Buffer createBuffer(VkBufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator);
Buffer createStagingBuffer(size_t size, VmaAllocator allocator);
Buffer createUBO(size_t size, VmaAllocator allocator);
Buffer createSSBO(size_t size, VmaAllocator allocator);
Buffer createVBO(size_t size, VmaAllocator allocator);
Buffer createIndirectBuffer(size_t size, VmaAllocator allocator);

VkDeviceAddress getBufferAddress(VkBuffer buffer, VkDevice device);

} // end namespace vke

#endif // VK_BUFFER_HPP
