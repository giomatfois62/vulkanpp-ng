#include "vk_buffer.hpp"
#include "vk_utils.hpp"

#include <cstring>

using namespace vke;

Buffer vke::createBuffer(VkBufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo, VmaAllocator allocator)
{
    Buffer buffer{ .info = bufferInfo };

    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer.handle, &buffer.allocation, &buffer.allocInfo));

    return buffer;
}

Buffer vke::createStagingBuffer(size_t size, VmaAllocator allocator)
{
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    return createBuffer(bufferInfo, allocInfo, allocator);
}

Buffer vke::createUBO(size_t size, VmaAllocator allocator)
{
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    return createBuffer(bufferInfo, allocInfo, allocator);
}

Buffer vke::createSSBO(size_t size, VmaAllocator allocator)
{
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    return createBuffer(bufferInfo, allocInfo, allocator);
}

VkDeviceAddress vke::getBufferAddress(VkBuffer buffer, VkDevice device)
{
    VkBufferDeviceAddressInfo addressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };

    return vkGetBufferDeviceAddress(device, &addressInfo);
}

void ShaderBuffers::update(uint32_t index, const void *data, size_t size, size_t offset)
{
    if (offset + size > this->size)
        recreate(buffers.size(), offset + size);

    memcpy((char*)buffers[index].allocInfo.pMappedData + offset, data, size);
}

void ShaderBuffers::create(uint32_t count, size_t size, VkDevice device, VmaAllocator allocator)
{
    cleanup();

    this->device = device;
    this->allocator = allocator;
    this->size = size;

    for (uint32_t i = 0; i < count; ++i) {
        Buffer buffer = type == BufferType::Uniform ? createUBO(size, allocator) : createSSBO(size, allocator);
        buffer.address = getBufferAddress(buffer.handle, device);
        buffers.push_back(buffer);
    }
}

void ShaderBuffers::recreate(uint32_t count, size_t size)
{
    cleanup();

    create(count, size, device, allocator);
}

void ShaderBuffers::cleanup()
{
    for (auto &buffer : buffers)
        vmaDestroyBuffer(allocator, buffer.handle, buffer.allocation);

    buffers.clear();
}
