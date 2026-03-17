#ifndef VK_SCENE_HPP
#define VK_SCENE_HPP

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "vk_mesh.hpp"
#include "vk_texture.hpp"
#include "vk_thread.hpp"

#include <queue>
#include <map>

namespace vke {

template<typename T>
struct SparseVector {
    SparseVector() { items.resize(1); } // reserve first item

    size_t insert(const T& item) {
        size_t id;

        if (freeIDs.size()) {
            id = freeIDs.front();
            freeIDs.pop();
            items[id] = item;
        } else {
            id = items.size();
            items.push_back(item);
        }

        return id;
    }

    void remove(size_t id) { freeIDs.push(id); }// TODO: cleanup?

    size_t dataSize() { return sizeof(T) * items.size(); }

    T *data() { return items.data(); }

    std::vector<T> items;
    std::queue<size_t> freeIDs;
};

class Scene
{
public:
    void init(VkDevice device, VkQueue queue, uint32_t queueFamilyIndex, uint32_t framesInFlight,
        VmaAllocator allocator, VkDescriptorSet bindlessDescriptorSet);
    void cleanup();

    Texture createTexture(const TextureData &data);
    uint32_t loadTexture(const std::string &path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    uint32_t storeTexture(const Texture &texture);
    uint32_t storeMaterial(const Material &material);
    uint32_t storePBRMaterial(const PBRMaterial &material);

    Model loadModel(std::vector<vke::Vertex> &vertices, const std::vector<uint32_t> &indices, const std::string &name);
    Model loadModel(const std::string &path, const std::string &name = "");
    Model loadOBJ(const std::string &path, const std::string &name = "");
    Model loadGLTF(const std::string &path, const std::string &name = "");

    std::map<std::string, vke::Model> models;
    SparseVector<Texture> textures;
    std::map<std::string, uint32_t> texturesMap;
    SparseVector<Material> materials;
    std::map<std::string, uint32_t> materialsMap;
    SparseVector<PBRMaterial> pbrMaterials;
    std::map<std::string, uint32_t> pbrMaterialsMap;

protected:
    VkDevice device;
    VkQueue queue;
    VmaAllocator allocator;
    VkCommandPool commandPool;
    uint32_t framesInFlight;
    VkDescriptorSet bindlessDescriptorSet;
    vke::Thread jobQueue;
};

} // end namespace vke

#endif // VK_SCENE_HPP
