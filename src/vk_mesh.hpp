#ifndef MESH_HPP
#define MESH_HPP

#include "vk_buffer.hpp"
#include "vk_volume.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <string>
#include <vector>

namespace vke {

struct Material {
    float ambient[4] = { 1.0f, 0.5f, 0.31f };
    float diffuse[4] = { 1.0f, 0.5f, 0.31f };
    float specular[4] = { 0.5f, 0.5f, 0.5f };
    float shininess = 32.0f;
    uint32_t ambientTex = 0;
    uint32_t diffuseTex = 0;
    uint32_t specularTex = 0;
    uint32_t normalTex = 0;
    uint32_t pad[3];
};

struct PBRMaterial {
    float baseColor[4] = { 1.0f, 1.0f, 1.0f };
    float metallic = 1.0f;
    float roughness = 1.0f;
    uint32_t baseColorTex = 0;
    uint32_t metallicRoughnessTex = 0;
    uint32_t normalTex = 0;
    uint32_t occlusionTex = 0;
    uint32_t pad[2];
    // TODO: emissive, transmission, glossiness, alphamask
};

struct VertexInputDescription {
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;
	VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 tangent; // TODO: use vec4
    glm::vec2 uv;
    glm::vec3 color; // TODO: use vec4 for alpha
    uint32_t material;

    static VertexInputDescription description();

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && uv == other.uv && material == other.material;
    }
};

struct Mesh {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    uint32_t firstVertex = 0;
    uint32_t vertexCount = 0;
    std::vector<uint32_t> materials = { 0 };
    vke::Volume volume;
    bool hasTangents = false;
};

struct Node {
    glm::mat4 localMatrix = glm::mat4(1.0f);
    glm::mat4 worldMatrix;
    int parentIndex = -1;
    int meshIndex = -1;
    int nodeId = -1;
};

struct GeometryData {
    std::vector<vke::Vertex> vertices;
    std::vector<uint32_t> indices;
};

struct InstanceData {
    uint32_t materialSet = 0;
    uint32_t isVisible = 0;
    uint32_t pad[2];
    glm::mat4 transform;
};

struct Model {
    void upload(std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices,
        uint32_t framesInFlight, VkDevice device, VmaAllocator allocator);
    void draw(VkCommandBuffer cmd, const std::vector<InstanceData> &instances,
        uint32_t frameIndex, VkPipelineLayout pipelineLayout, uint32_t pushConstantOffset, bool multiDraw = true);
    void updateInstances(const std::vector<InstanceData> &instances, uint32_t frameIndex);
    void bind(VkCommandBuffer cmd);
    void cleanup();
    void computeVolume();

    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    size_t vertexCount;
    size_t indexCount;
    vke::Volume volume;
    std::string name;
    vke::SSBOs instanceBuffer;
    std::vector<VkDrawIndexedIndirectCommand> indirectDrawCommands;
    vke::IndirectBuffers drawCommandsBuffer;
    Buffer vertexBuffer;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
};

void computeTangents(std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices, uint32_t firstIndex, uint32_t indexCount);
vke::Volume computeVolume(const std::vector<Vertex> &vertices);
void updateNodes(std::vector<Node> &nodes, const glm::mat4 transform = glm::mat4(1.0f));

} // end namespace vke

#endif // MESH_HPP
