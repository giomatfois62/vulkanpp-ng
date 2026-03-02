#ifndef MESH_HPP
#define MESH_HPP

#include "vk_buffer.hpp"
#include "vk_volume.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

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
    uint32_t bumpTex = 0;
    uint32_t pad[3];
};

struct VertexInputDescription {
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;
	VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec2 uv;
    glm::vec3 color;
    uint32_t material;

    static VertexInputDescription description();

    bool operator==(const Vertex& other) const {
            return pos == other.pos && color == other.color && uv == other.uv && material == other.material;
    }
};

struct InstanceDrawData {
    uint32_t material = 0;
    uint32_t isVisible = 1;
    uint32_t pad[2];
    glm::mat4 transform;
};

struct Mesh {
    Mesh();
    Mesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);

	std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t material = 0;
    uint32_t visibleInstances = 0;
    Buffer vertexBuffer;
    vke::Volume volume;

    std::vector<vke::InstanceDrawData> drawData;
    vke::ShaderBuffers drawDataBuffers;

    void computeVolume();
    void computeTangents();
    void draw(VkCommandBuffer cmd, uint32_t count);
    void upload(VmaAllocator allocator);
    void cleanup(VmaAllocator allocator);
    void updateDrawData(uint32_t frameIndex);
};

struct Model {
    Model();
    Model(const std::vector<vke::Mesh> &meshes);

    std::vector<Mesh> meshes;
    vke::Volume volume;

    void draw(VkCommandBuffer cmd, uint32_t count);
    void upload(VmaAllocator allocator);
    void cleanup(VmaAllocator allocator);
    void computeVolume();
};

}

#endif // MESH_HPP
