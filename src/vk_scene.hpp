#ifndef VK_SCENE_HPP
#define VK_SCENE_HPP

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "vk_camera.hpp"
#include "vk_light.hpp"
#include "vk_mesh.hpp"
#include "vk_image.hpp"
#include "vk_thread.hpp"

#include <map>

namespace vke {

template<typename T>
struct Resources {
    Resources() { items.resize(1); itemsMap.insert({ "default", 0 }); } // reserve first item

    void setDefault(const T& item) { items[0] = item; }

    T& get(const std::string &name) { return items[itemsMap[name]]; }

    size_t insert(const T& item, const std::string &name)
    {
        size_t id;

        if (freeIDs.size()) {
            id = freeIDs.front();
            freeIDs.pop();
            items[id] = item;
        } else {
            id = items.size();
            items.push_back(item);
        }

        itemsMap.insert({ generateResourceName(name), id });

        return id;
    }

    void remove(const std::string &name)
    {
        auto it = itemsMap.find(name);

        if (it != itemsMap.end()) {
            freeIDs.push(it->second);
            items[it->second] = {};
            itemsMap.erase(it);
        }
    }

    size_t dataSize() { return sizeof(T) * items.size(); }

    T* data() { return items.data(); }

    std::string generateResourceName(const std::string &baseName)
    {
        std::string name = baseName.empty() ? "resource" : baseName;

        if (itemsMap.find(name) == itemsMap.end())
            return name;

        // generate a name for the resource
        uint32_t counter = 0;
        do {
            name = name + "." + std::to_string(counter++);
        } while (itemsMap.find(name) != itemsMap.end());

        return name;
    }

    std::vector<T> items;
    std::queue<size_t> freeIDs;
    std::map<std::string,uint32_t> itemsMap;
};


class Scene
{
public:
    void init(VkDevice device, uint32_t queueFamilyIndex, VmaAllocator allocator);
    void cleanup();

    Texture createTexture(const TextureData &data);
    uint32_t loadTexture(const std::string &path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    uint32_t storeTexture(const Texture &texture);
    uint32_t storeMaterial(const Material &material, const std::string &name);
    uint32_t storePBRMaterial(const PBRMaterial &material, const std::string &name);
    uint32_t storeModel(const Model &model);

    Model loadModel(std::vector<vke::Vertex> &vertices, const std::vector<uint32_t> &indices, const std::string &name);
    Model loadModel(const std::string &path);
    Model loadOBJ(const std::string &path);
    Model loadGLTF(const std::string &path);

    Camera camera;

    std::vector<vke::Light> lights; // from ecs world?
    Resources<Model> models;
    Resources<Texture> textures;
    Resources<Material> materials;
    Resources<PBRMaterial> pbrMaterials;

    // shader buffers
    vke::UBOs cameraBuffers;
    vke::SSBOs lightBuffers;
    vke::SSBOs materialBuffers;
    vke::SSBOs pbrMaterialBuffers;

    // bindless descriptors
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout bindlessDescriptorSetLayout;
    VkDescriptorSet bindlessDescriptorSet;

protected:
    void createBindlessDescriptors();

    VkDevice device;
    VkQueue queue;
    VmaAllocator allocator;
    VkCommandPool commandPool;
    vke::Thread jobQueue;
};

} // end namespace vke

#endif // VK_SCENE_HPP
