#include "vk_mesh.hpp"

#include <cstring>
#include <iostream>

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

// https://www.opengl-tutorial.org/intermediate-tutorials/tutorial-13-normal-mapping/#computing-the-tangents-and-bitangents
void vke::computeTangents(std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices, uint32_t firstIndex, uint32_t indexCount)
{
    // set current tangents/bitangents zero
    for (auto &v : vertices) {
        v.tangent = { 0.f, 0.f, 0.f };
        //v.bitangent = { 0.f, 0.f, 0.f };
    }

    for (size_t i = firstIndex; i < indexCount; i += 3) {
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

Volume vke::computeVolume(const std::vector<Vertex> &vertices)
{
    Volume volume(glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX));

    for(Vertex v : vertices)
        volume.updateDimensions(v.pos);

    for (size_t i = 0; i < 3; ++i) {
        if ((volume.max[i]-volume.min[i]) == 0) {
            volume.max[i] += 0.0001;
            volume.min[i] -= 0.0001;
        }
    }

    return volume;
}

void Model::upload(std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices,
    uint32_t framesInFlight, VkDevice device, VmaAllocator allocator)
{
    this->allocator = allocator;
    this->device = device;
    this->vertexCount = vertices.size();
    this->indexCount = indices.size();

    // allocate vertex buffer
    VkDeviceSize vBufSize = vertices.size() * sizeof(Vertex);
    VkDeviceSize iBufSize = indices.size() * sizeof(uint32_t);

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

    // generate tangents for vertices
    for (auto &mesh : meshes) {
        if (!mesh.hasTangents)
            computeTangents(vertices, indices, mesh.firstIndex, mesh.indexCount);
    }

    // compute model volume
    updateNodes(nodes);
    computeVolume();

    // copy vertex data
    void *data;

    vmaMapMemory(allocator, vertexBuffer.allocation, &data);
        memcpy(data, vertices.data(), vBufSize);
        memcpy(((char*)data) + vBufSize, indices.data(), iBufSize);
    vmaUnmapMemory(allocator, vertexBuffer.allocation);

    // create instances and indirectDrawCommands buffers
    instanceBuffer.create(framesInFlight, sizeof(InstanceData), device, allocator);
    drawCommandsBuffer.create(framesInFlight, sizeof(VkDrawIndexedIndirectCommand), device, allocator);

    std::cout << "Uploaded model \"" << name << "\" with " << vertexCount << " vertices, " <<
        indexCount / 3 << " triangles, " << nodes.size() << " nodes, " <<
        meshes.size() << " meshes" << std::endl;
}

void Model::cleanup()
{
    if (allocator != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, vertexBuffer.handle, vertexBuffer.allocation);

        drawCommandsBuffer.cleanup();
        instanceBuffer.cleanup();

        nodes.clear();
        meshes.clear();
    }
}

void Model::computeVolume()
{
    volume = Volume(glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX));

    for (auto &node : nodes) {
        if (node.meshIndex >= 0) {
            Volume nodeVolume = meshes[node.meshIndex].volume.transformed(node.worldMatrix);
            volume = volume.minimumBoundingBox(nodeVolume);
        }
    }
}

void Model::draw(VkCommandBuffer cmd, const std::vector<InstanceData> &instances,
    uint32_t frameIndex, VkPipelineLayout pipelineLayout, uint32_t pushConstantOffset)
{
    if (!instances.size())
        return;

    updateInstances(instances, frameIndex);

    bind(cmd);

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, pushConstantOffset,
        sizeof(VkDeviceAddress), &instanceBuffer.deviceAddress(frameIndex));

    vkCmdDrawIndexedIndirect(cmd, drawCommandsBuffer.buffer(frameIndex),
        0, indirectDrawCommands.size(), sizeof(VkDrawIndexedIndirectCommand));

    // multiple draw calls
    /*
    for (size_t i = 0; i < indirectDrawCommands.size(); ++i) {
        vkCmdDrawIndexedIndirect(cmd, drawCommandsBuffer.buffer(frameIndex),
            i * sizeof(VkDrawIndexedIndirectCommand), 1, sizeof(VkDrawIndexedIndirectCommand));
    }*/

    // without draw indirect
    /*
    for (size_t i = 0; i < meshes.size(); ++i) {
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, pushConstantOffset,
            sizeof(VkDeviceAddress), &instanceBuffers[i].deviceAddress(frameIndex));
        vkCmdDrawIndexed(cmd, meshes[i].indexCount, drawData[i].size(), meshes[i].firstIndex, 0, 0);
    }*/
}

void Model::updateInstances(const std::vector<InstanceData> &instances, uint32_t frameIndex)
{
    // create instance matrices for all meshes
    std::vector<std::vector<InstanceData>> drawData(meshes.size());

    for (auto & instance : instances) {
        // make a copy of nodes array to compute world matrices
        auto modelNodes = nodes;
        updateNodes(modelNodes, instance.transform);

        for (auto &node : modelNodes) {
            if (node.meshIndex >= 0) {
                auto &mesh = meshes[node.meshIndex];

                drawData[node.meshIndex].push_back({
                    .materialSet = mesh.materials[instance.materialSet],
                    .transform = node.worldMatrix
                });
            }
        }
    }

    // create indirect draw commands
    indirectDrawCommands.resize(drawData.size());
    uint32_t firstInstance = 0;
    for (size_t i = 0; i < drawData.size(); ++i) {
        indirectDrawCommands[i] = {
            .indexCount = meshes[i].indexCount,
            .instanceCount = static_cast<uint32_t>(drawData[i].size()),
            .firstIndex = meshes[i].firstIndex,
            .firstInstance = firstInstance
        };
        firstInstance += static_cast<uint32_t>(drawData[i].size());
    }
    uint32_t totalInstances = firstInstance;

    // upload instances to SSBO
    instanceBuffer.recreate(instanceBuffer.buffers.size(), sizeof(InstanceData) * totalInstances);
    size_t drawDataOffset = 0;
    for (size_t i = 0; i < drawData.size(); ++i) {
        size_t dataSize = sizeof(InstanceData) * drawData[i].size();
        instanceBuffer.update(frameIndex, drawData[i].data(), dataSize, drawDataOffset);
        drawDataOffset += dataSize;
    }

    drawCommandsBuffer.update(frameIndex, indirectDrawCommands.data(),
        sizeof(VkDrawIndexedIndirectCommand) * indirectDrawCommands.size());
}

void Model::bind(VkCommandBuffer cmd)
{
    VkDeviceSize offset = 0;
    VkDeviceSize vBufSize = sizeof(Vertex) * vertexCount;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.handle, &offset);
    vkCmdBindIndexBuffer(cmd, vertexBuffer.handle, vBufSize, VK_INDEX_TYPE_UINT32);
}

void vke::updateNodes(std::vector<Node> &nodes, const glm::mat4 transform)
{
    if (!nodes.size())
        return;

    for (auto &node : nodes){
        if (node.parentIndex < 0)
            node.worldMatrix = transform * node.localMatrix;
        else
            node.worldMatrix = nodes[node.parentIndex].worldMatrix * node.localMatrix;
    }
}
