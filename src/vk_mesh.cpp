#include "vk_mesh.hpp"

#include <cstring>

using namespace vke;

vke::VertexInputDescription Vertex::description()
{
    VertexInputDescription description;

    description.bindings = {
        { 0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX }
    };

    description.attributes = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos) },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) },
        { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent) },
        //{ 3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, bitangent) },
        { 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) },
        { 4, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color) },
        { 5, 0, VK_FORMAT_R32_UINT, offsetof(Vertex, material) }
    };

    return description;
}

Mesh::Mesh()
{

}

Mesh::Mesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices) :
    vertices(vertices), indices(indices)
{
    computeVolume();
    computeTangents();
}

void Mesh::computeVolume()
{
    glm::vec3 min = {FLT_MAX, FLT_MAX, FLT_MAX};
    glm::vec3 max = {FLT_MIN, FLT_MIN, FLT_MIN};

    for(Vertex v : vertices) {
        min[0] = fminf(min[0], v.pos[0]);
        min[1] = fminf(min[1], v.pos[1]);
        min[2] = fminf(min[2], v.pos[2]);

        max[0] = fmaxf(max[0], v.pos[0]);
        max[1] = fmaxf(max[1], v.pos[1]);
        max[2] = fmaxf(max[2], v.pos[2]);
    }

    for (size_t i = 0; i < 3; ++i) {
        if ((max[i]-min[i]) == 0) {
            max[i] += 0.0001;
            min[i] -= 0.0001;
        }
    }

    volume = Volume(min, max);
}

// https://www.opengl-tutorial.org/intermediate-tutorials/tutorial-13-normal-mapping/#computing-the-tangents-and-bitangents
void Mesh::computeTangents()
{
    // set current tangents/bitangents zero
    for (auto &v : vertices) {
        v.tangent = { 0.f, 0.f, 0.f };
        //v.bitangent = { 0.f, 0.f, 0.f };
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        glm::vec3 &v0 = vertices[indices[i+0]].pos;
        glm::vec3 &v1 = vertices[indices[i+1]].pos;
        glm::vec3 &v2 = vertices[indices[i+2]].pos;

        glm::vec2 &uv0 = vertices[indices[i+0]].uv;
        glm::vec2 &uv1 = vertices[indices[i+1]].uv;
        glm::vec2 &uv2 = vertices[indices[i+2]].uv;

        glm::vec3 deltaPos1 = v1-v0;
        glm::vec3 deltaPos2 = v2-v0;

        glm::vec2 deltaUV1 = uv1-uv0;
        glm::vec2 deltaUV2 = uv2-uv0;

        float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
        glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;
        //glm::vec3 bitangent = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x) * r;

        // average results
        for (size_t j = 0; j < 3; ++j) {
            vertices[indices[i+j]].tangent += tangent;
            //vertices[indices[i+j]].bitangent += bitangent;
        }
    }
}

void Mesh::draw(VkCommandBuffer cmd, uint32_t count)
{
    VkDeviceSize offset = 0;
    VkDeviceSize vBufSize = sizeof(Vertex) * vertices.size();
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.handle, &offset);
    vkCmdBindIndexBuffer(cmd, vertexBuffer.handle, vBufSize, VK_INDEX_TYPE_UINT32);

    // mesh data pushconstants
    //vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(VkDeviceAddress), &shaderDataBuffers[currentFrameIndex].address);

    // draw instanced
    vkCmdDrawIndexed(cmd, indices.size(), count, 0, 0, 0);
}

void Mesh::upload(VmaAllocator allocator)
{
    VkDeviceSize vBufSize = vertices.size() * sizeof(Vertex);
    VkDeviceSize iBufSize = indices.size() * sizeof(uint32_t);

    // allocate vertex buffer
    vertexBuffer = createBuffer(
        VkBufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = vBufSize + iBufSize,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        },
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_CPU_TO_GPU
        },
        allocator
    );

    // copy vertex data
    void *data;

    vmaMapMemory(allocator, vertexBuffer.allocation, &data);
        memcpy(data, vertices.data(), vBufSize);
        memcpy(((char*)data) + vBufSize, indices.data(), iBufSize);
    vmaUnmapMemory(allocator, vertexBuffer.allocation);
}

void vke::Mesh::cleanup(VmaAllocator allocator)
{
    vmaDestroyBuffer(allocator, vertexBuffer.handle, vertexBuffer.allocation);

    drawDataBuffers.cleanup(allocator);
}

void Mesh::updateDrawData(uint32_t frameIndex)
{
    size_t dataSize = drawData.size() * sizeof(InstanceDrawData);
    drawDataBuffers.update(frameIndex, drawData.data(), dataSize);
}

Model::Model(const std::vector<Mesh> &meshes) :
    meshes(meshes)
{
    computeVolume();
}

Model::Model()
{

}

void Model::draw(VkCommandBuffer cmd, uint32_t count)
{
    for (auto &mesh : meshes)
        mesh.draw(cmd, count);
}

void vke::Model::upload(VmaAllocator allocator)
{
    for (auto &mesh : meshes)
        mesh.upload(allocator);
}

void Model::cleanup(VmaAllocator allocator)
{
    for (auto &mesh : meshes)
        mesh.cleanup(allocator);
}

void Model::computeVolume()
{
    if (meshes.size()) {
        volume = meshes[0].volume;
        for (size_t i = 1; i < meshes.size(); ++i)
            volume = volume.minimumBoundingBox(meshes[i].volume);
    }
}
