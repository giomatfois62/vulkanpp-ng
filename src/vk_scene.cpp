#include "vk_scene.hpp"
#include "vk_core.hpp"
#include "vk_utils.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace std {

template<> struct hash<vke::Vertex> {
    size_t operator()(vke::Vertex const& vertex) const {
        return ((hash<glm::vec3>()(vertex.pos) ^
            (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
            (hash<glm::vec2>()(vertex.uv) << 1) ^
            (hash<uint32_t>()(vertex.material) << 1);
    }
};

} // end namespace std

using namespace vke;

void Scene::init(VkDevice device, uint32_t queueFamilyIndex, VmaAllocator allocator)
{
    this->device = device;
    this->allocator = allocator;

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    commandPool = createCommandPool(queueFamilyIndex, device);

    createBindlessDescriptors();

    cameraBuffers.create(framesInFlightCount, 1, device, allocator);
    lightBuffers.create(framesInFlightCount, 1, device, allocator);
    lightClusterBuffers.create(framesInFlightCount, 1, device, allocator);
    materialBuffers.create(framesInFlightCount, 1, device, allocator);
    pbrMaterialBuffers.create(framesInFlightCount, 1, device, allocator);
    lightClusterInfoBuffers.create(framesInFlightCount, sizeof(LightClusterInfo), device, allocator);
}

void Scene::createBindlessDescriptors()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, STORAGE_COUNT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SAMPLER_COUNT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, IMAGE_COUNT},
    };

    VkDescriptorPoolCreateInfo poolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VK_CHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool));

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {STORAGE_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, STORAGE_COUNT, VK_SHADER_STAGE_ALL, nullptr},
        {SAMPLER_BINDING, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, SAMPLER_COUNT, VK_SHADER_STAGE_ALL, nullptr},
        {IMAGE_BINDING, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, IMAGE_COUNT, VK_SHADER_STAGE_ALL, nullptr}
    };

    std::vector<VkDescriptorBindingFlags> bindingFlags = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindingFlags.size()),
        .pBindingFlags = bindingFlags.data()
    };

    VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &setLayoutBindingFlags,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    VK_CHECK(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &bindlessDescriptorSetLayout));

    std::vector<VkDescriptorSetLayout> sets = { bindlessDescriptorSetLayout };

    VkDescriptorSetAllocateInfo setAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = sets.data()
    };

    VK_CHECK(vkAllocateDescriptorSets(device, &setAllocateInfo, &bindlessDescriptorSet));
}

void Scene::cleanup()
{
    for (auto &texture : textures.items)
        texture.cleanup();

    for (auto &model : models.items)
        model.cleanup();

    vkDestroyCommandPool(device, commandPool, nullptr);

    vkDestroyDescriptorSetLayout(device, bindlessDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    cameraBuffers.cleanup();
    lightBuffers.cleanup();
    lightClusterBuffers.cleanup();
    materialBuffers.cleanup();
    pbrMaterialBuffers.cleanup();
    lightClusterInfoBuffers.cleanup();
}

Texture Scene::createTexture(const TextureData &data)
{
    Texture texture{
        .device = device,
        .allocator = allocator,
        .name = data.path
    };

    Buffer uploadbuffer = createStagingBuffer(data.imageSize, allocator);
    memcpy(uploadbuffer.allocInfo.pMappedData, data.pixels.data(), data.imageSize);

    texture.image = createImage(
        VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = data.format,
            .extent = data.extent,
            .mipLevels = data.mipLevels,
            .arrayLayers = data.arrayLayers,
            .samples = VK_SAMPLE_COUNT_1_BIT, // TODO: support multisampling
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        },
        VmaAllocationCreateInfo{ .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE },
        allocator
    );

    VkImageSubresourceRange subresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = data.mipLevels,
        .layerCount = data.arrayLayers
    };

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture.image.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D, // TODO: support cubemaps and arrays
        .format = data.format,
        .subresourceRange = subresourceRange
    };

    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &texture.image.view));

    VkCommandBuffer cmd = createCommandBuffer(true, commandPool, device);

    changeImageLayout(cmd, texture.image.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange
    );

    if (!data.hasMipmaps) {
        VkBufferImageCopy copyRegion{
            .imageSubresource = VkImageSubresourceLayers{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = data.arrayLayers
            },
            .imageExtent = data.extent
        };

        vkCmdCopyBufferToImage(cmd, uploadbuffer.handle, texture.image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        // TODO: support layers mipmap generation
        generateMipmaps(cmd, texture.image.handle, {data.extent.width, data.extent.height}, data.mipLevels);
    } else {
        std::vector<VkBufferImageCopy> copyRegions;

        for (uint32_t layer = 0; layer < data.arrayLayers; ++layer) {
            for (uint32_t mip = 0; mip < data.mipLevels; ++mip) {
                size_t mipOffset = data.offsets[layer][mip];

                copyRegions.push_back(
                    VkBufferImageCopy{
                        .bufferOffset = mipOffset,
                        .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = mip, .baseArrayLayer = layer, .layerCount = 1 },
                        .imageExtent{ .width = data.extent.width >> mip, .height = data.extent.height >> mip, .depth = 1 },
                    }
                );
            }
        }

        vkCmdCopyBufferToImage(cmd, uploadbuffer.handle, texture.image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

        changeImageLayout(cmd, texture.image.handle,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresourceRange
        );
    }

    submitCommandBuffer(cmd, queue, commandPool, device, true);

    vmaDestroyBuffer(allocator, uploadbuffer.handle, uploadbuffer.allocation);

    texture.sampler = createDefaultSampler(device);

    return texture;
}

uint32_t Scene::loadTexture(const std::string &path, VkFormat format)
{
    TextureData data = loadTextureData(path, format);

    return storeTexture(createTexture(data));
}

uint32_t Scene::storeTexture(const Texture &texture)
{
    uint32_t textureID = textures.insert(texture, texture.name.empty() ? "texture" : texture.name);

    VkDescriptorImageInfo imageInfo{
        .sampler = texture.sampler,
        .imageView = texture.image.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = bindlessDescriptorSet,
        .dstBinding = SAMPLER_BINDING,
        .dstArrayElement = static_cast<uint32_t>(textureID),
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo
    };

    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    return textureID;
}

uint32_t Scene::storeMaterial(const Material &material, const std::string &name)
{
    return materials.insert(material, name.empty() ? "material" : name);
}

uint32_t Scene::storePBRMaterial(const PBRMaterial &material, const std::string &name)
{
    return pbrMaterials.insert(material, name.empty() ? "material" : name);
}

uint32_t Scene::storeModel(const Model &model)
{
    return models.insert(model, model.name.empty() ? "model" : model.name);
}

Model Scene::loadModel(const std::string &path)
{
    std::string extension = getFileExtension(path);

    if (extension == "obj" || extension == "OBJ") {
        return loadOBJ(path);
    } else if (extension == "gltf" || extension == "GLTF" || extension == "glb" || extension == "GLB") {
        return loadGLTF(path);
    } else {
        return loadGLTF(path); // fallback on gltf format
    }
}

Model Scene::loadOBJ(const std::string &path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string baseDir = getFileDirectory(path);
    std::string modelName = getFileName(path);

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, nullptr, path.c_str(), baseDir.c_str())) {
        throw std::runtime_error("Failed loading obj from " + path);
    }

    std::vector<uint32_t> loadedMaterials;
    std::map<std::string,uint32_t> loadedTextures;

    auto loadMaterialTexture = [&](const std::string &fileName, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB) {
        uint32_t id = 0;

        if (fileName.empty()) return id;

        if (loadedTextures.find(fileName) == loadedTextures.end()) {
            id = loadTexture(baseDir + "/" + fileName, format);
            loadedTextures[fileName] = id;
        } else {
            id = loadedTextures[fileName];
        }

        return id;
    };

    for (auto &mat : materials) {
        Material material{
            .ambient = { mat.ambient[0], mat.ambient[1], mat.ambient[2] },
            .diffuse = { mat.diffuse[0], mat.diffuse[1], mat.diffuse[2] },
            .specular = { mat.specular[0], mat.specular[1], mat.specular[2] },
            .shininess = mat.shininess,
            .ambientTex = loadMaterialTexture(mat.ambient_texname),
            .diffuseTex = loadMaterialTexture(mat.diffuse_texname),
            .specularTex = loadMaterialTexture(mat.specular_texname),
            // prefer normal_tex, specify linear image format
            .normalTex = loadMaterialTexture(mat.normal_texname.size() ? mat.normal_texname : mat.bump_texname, VK_FORMAT_R8G8B8A8_UNORM),
        };

        loadedMaterials.push_back(storeMaterial(material, modelName + "_" + mat.name));
    }

    Model model{ .name = modelName };
    std::unordered_map<Vertex, uint32_t> uniqueVertices;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (auto &shape : shapes) {
        uint32_t firstIndex = static_cast<uint32_t>(vertices.size());
        uint32_t firstVertex = static_cast<uint32_t>(indices.size());
        uint32_t indexCount = 0;
        uint32_t vertexCount = 0;
        Volume volume(glm::vec3(FLT_MAX), glm::vec3(-FLT_MAX));

        for (auto& index : shape.mesh.indices) {
            Vertex v;

            v.pos = {
                attrib.vertices[index.vertex_index * 3],
                attrib.vertices[index.vertex_index * 3 + 1],
                attrib.vertices[index.vertex_index * 3 + 2]
            };

            v.normal = {
                attrib.normals[index.normal_index * 3],
                attrib.normals[index.normal_index * 3 + 1],
                attrib.normals[index.normal_index * 3 + 2]
            };

            v.uv = {
                attrib.texcoords[index.texcoord_index * 2],
                1.0f - attrib.texcoords[index.texcoord_index * 2 + 1]
            };

            v.color = {
                attrib.colors[index.vertex_index * 3],
                attrib.colors[index.vertex_index * 3 + 1],
                attrib.colors[index.vertex_index * 3 + 2]
            };

            volume.updateDimensions(v.pos);

            uint32_t material = shape.mesh.material_ids[indexCount/3];
            v.material = loadedMaterials.size() ? loadedMaterials[material] : 0;

            if (uniqueVertices.count(v) == 0) {
                uniqueVertices[v] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(v);
                vertexCount++;
            }

            indices.push_back(uniqueVertices[v]);
            indexCount++;
        }

        model.nodes.push_back({
            .meshIndex = static_cast<int>(model.meshes.size()),
        });

        model.meshes.push_back({
            .firstIndex = firstIndex,
            .indexCount = indexCount,
            .firstVertex = firstVertex,
            .vertexCount = vertexCount,
            .materials = { shape.mesh.material_ids.size() ? loadedMaterials[shape.mesh.material_ids[0]] : 0 },
            .volume = volume
        });
    }

    model.upload(vertices, indices, framesInFlightCount, device, allocator);

    return model;
}

bool loadImageData(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning,
    int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
{
    // KTX files will be handled by our own code
    std::string ext = getFileExtension(image->uri);

    if (ext == "ktx" || ext == "ktx2")
        return true;

    return tinygltf::LoadImageData(image, imageIndex, error, warning, req_width, req_height, bytes, size, userData);
}

void loadGLTFMesh(int meshIndex, Model &model, tinygltf::Model &gltfModel, std::map<int,std::vector<int>> &loadedMeshes,
    std::map<int,uint32_t> &loadedMaterials, std::vector<vke::Vertex> &vertices, std::vector<uint32_t> &indices)
{
    auto &mesh = gltfModel.meshes[meshIndex];
    std::vector<int> generatedMeshIndices;

    std::cout << "Loading mesh " << mesh.name << std::endl;

    for (const auto& primitive : mesh.primitives) {
        auto &posAccessor = gltfModel.accessors[primitive.attributes.at("POSITION")];
        size_t vertexCount = posAccessor.count;
        uint32_t firstIndex = static_cast<uint32_t>(indices.size());
        uint32_t baseVertexIndex = static_cast<uint32_t>(vertices.size());
        glm::vec3 minPos = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
        glm::vec3 maxPos = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
        Volume volume(minPos, maxPos);

        auto getBuffer = [&](const std::string &type){
            const float *buf = nullptr;

            if (primitive.attributes.find(type) != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.at(type)];
                const tinygltf::BufferView& view = gltfModel.bufferViews[accessor.bufferView];
                buf = reinterpret_cast<const float*>(&gltfModel.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
            }

            return buf;
        };

        // get primitive vertices
        const float *posBuffer = getBuffer("POSITION");
        const float *normBuffer = getBuffer("NORMAL");
        std::string name;
        const float *tanBuffer = getBuffer("TANGENT");
        const float *uvBuffer = getBuffer("TEXCOORD_0");
        const float *colorBuffer = getBuffer("COLOR_0");
        uint32_t colorComponents;
        // TODO: JOINTS_0, WEIGHTS_0

        if (colorBuffer)
            colorComponents = gltfModel.accessors[primitive.attributes.at("COLOR_0")].type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;

        for (size_t i = 0; i < vertexCount; ++i) {
            Vertex v {
                .pos = glm::make_vec3(&posBuffer[i*3]),
                .normal = normBuffer ? glm::make_vec3(&normBuffer[i*3]) : glm::vec3(0.0f),
                .tangent = tanBuffer ? glm::make_vec3(&tanBuffer[i*4]) : glm::vec3(0.0f),
                .uv = uvBuffer ? glm::make_vec2(&uvBuffer[i*2]) : glm::vec2(0.0f),
                .color = colorBuffer ? (colorComponents == 3 ? glm::make_vec3(&colorBuffer[i*3]) : glm::make_vec3(&colorBuffer[i*4])) : glm::vec3(1.0f)
            };

            vertices.push_back(v);
        }

        // get primitive indices
        const tinygltf::Accessor &indexAccessor = gltfModel.accessors[primitive.indices];
        const tinygltf::BufferView &indexBufferView = gltfModel.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer &indexBuffer = gltfModel.buffers[indexBufferView.buffer];

        size_t indexCount  = indexAccessor.count;

        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            const uint16_t* buf = reinterpret_cast<const uint16_t*>(&indexBuffer.data[indexAccessor.byteOffset + indexBufferView.byteOffset]);
            for (size_t i = 0; i < indexCount; i++)
                indices.push_back(buf[i] + baseVertexIndex);
        }

        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            const uint32_t* buf = reinterpret_cast<const uint32_t*>(&indexBuffer.data[indexAccessor.byteOffset + indexBufferView.byteOffset]);
            for (size_t i = 0; i < indexCount; i++)
                indices.push_back(buf[i] + baseVertexIndex);
        }

        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            const uint8_t* buf = reinterpret_cast<const uint8_t*>(&indexBuffer.data[indexAccessor.byteOffset + indexBufferView.byteOffset]);
            for (size_t i = 0; i < indexCount; i++)
                indices.push_back(buf[i] + baseVertexIndex);
        }

        Mesh mesh{
            .firstIndex = firstIndex,
            .indexCount = static_cast<uint32_t>(indexCount),
            .firstVertex = baseVertexIndex,
            .vertexCount = static_cast<uint32_t>(vertexCount),
            .volume = volume,
            .hasTangents = (tanBuffer != nullptr)
        };

        if (primitive.material >= 0)
            mesh.materials = { loadedMaterials[primitive.material] };

        generatedMeshIndices.push_back(model.meshes.size());
        model.meshes.push_back(mesh);
    }

    loadedMeshes[meshIndex] = generatedMeshIndices;
}

void loadGLTFNode(int parentIndex, int nodeId, const tinygltf::Node &node, Model &model,
    tinygltf::Model &gltfModel, std::map<int,std::vector<int>> &loadedMeshes)
{
    std::cout << "Loading node " << node.name << std::endl;

    // new node index
    int nodeIndex = model.nodes.size();

    // compute node local matrix
    glm::vec3 translation = glm::vec3(0.0f);
    glm::quat rotation = {};
    glm::vec3 scale = glm::vec3(1.0f);
    glm::mat4 matrix = glm::mat4(1.0f);

    if (node.translation.size() == 3)
        translation = glm::make_vec3(node.translation.data());

    if (node.rotation.size() == 4)
        rotation = glm::make_quat(node.rotation.data());

    if (node.scale.size() == 3)
        scale = glm::make_vec3(node.scale.data());

    if (node.matrix.size() == 16)
        matrix = glm::make_mat4(node.matrix.data());

    matrix = glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;

    model.nodes.push_back({
        .localMatrix = matrix,
        .parentIndex = parentIndex,
        .nodeId = nodeId, // gltf node id, used for skin/animations
    });

    if (node.mesh >= 0) {
        if (loadedMeshes[node.mesh].size() > 1) {
            // generate children nodes if mesh has multiple submeshes
            for (auto &meshIndex : loadedMeshes[node.mesh]) {
                model.nodes.push_back({
                    .parentIndex = nodeIndex,
                    .meshIndex = meshIndex,
                    .nodeId = nodeId
                });
            }
        } else {
            // assign mesh to node
            model.nodes[nodeIndex].meshIndex = loadedMeshes[node.mesh][0];
        }
    }

    // recursive call to load children
    if (node.children.size() > 0) {
        for (size_t i = 0; i < node.children.size(); i++)
            loadGLTFNode(nodeIndex, node.children[i], gltfModel.nodes[node.children[i]], model, gltfModel, loadedMeshes);
    }
}

Model Scene::loadGLTF(const std::string &path)
{
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    bool res;

    loader.SetImageLoader(loadImageData, nullptr);

    if (getFileExtension(path) == "glb") {
        res = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, path);
    } else {
        res = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, path);
    }

    if (!warn.empty())
        std::cout << "GLTF Warning: " << warn << std::endl;

    if (!err.empty() || !res)
        std::cerr << "GLTF Error: " << err << std::endl;

    if (!res)
        throw std::runtime_error("Failed to load glTF model from " + path);

    // first load all materials and textures
    std::string modelName = getFileName(path);
    std::map<int,uint32_t> loadedMaterials;
    std::map<int,uint32_t> loadedTextures;

    auto loadMaterialTexture = [&](int textureIndex, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB) {
        uint32_t id = 0;

        if (textureIndex < 0)
            return id;

        if (loadedTextures.find(textureIndex) != loadedTextures.end())
            return loadedTextures[textureIndex];

        auto gltfTexture = gltfModel.textures[textureIndex];
        auto imageIndex = gltfTexture.source;
        auto image = gltfModel.images[imageIndex];

        std::cout << "Loading texture " << gltfTexture.name << ", image: " << image.name << std::endl;

        if (image.bufferView >= 0) {
            // image embedded in model file
            const auto& bufferView = gltfModel.bufferViews[image.bufferView];
            const auto& buffer = gltfModel.buffers[bufferView.buffer];

            if (image.mimeType == "image/ktx2" || image.mimeType == "image/ktx") {
                const uint8_t* ktx2Data = buffer.data.data() + bufferView.byteOffset;
                size_t ktx2Size = bufferView.byteLength;

                id = storeTexture(createTexture(loadTextureDataKtx(ktx2Data, ktx2Size)));
            } else {
                TextureData textureData {
                    .pixels = image.image,
                    .imageSize = image.width * image.height * image.component * sizeof(uint8_t),
                    .format = format,
                    .extent = {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height), 1},
                };

                id = storeTexture(createTexture(textureData));
            }
        } else {
            // image stored in separate file
            std::string imagePath = getFileDirectory(path) + "/" + image.uri;

            id = loadTexture(imagePath, format);
        }

        loadedTextures[textureIndex] = id;

        return id;
    };

    for (size_t index = 0; index < gltfModel.materials.size(); ++index) {
        auto &gltfMaterial = gltfModel.materials[index];

        std::cout << "Loading material " << gltfMaterial.name << std::endl;

        PBRMaterial material {
            .metallic = static_cast<float>(gltfMaterial.pbrMetallicRoughness.metallicFactor),
            .roughness = static_cast<float>(gltfMaterial.pbrMetallicRoughness.roughnessFactor),
            .baseColorTex = loadMaterialTexture(gltfMaterial.pbrMetallicRoughness.baseColorTexture.index),
            .metallicRoughnessTex = loadMaterialTexture(gltfMaterial.pbrMetallicRoughness.metallicRoughnessTexture.index, VK_FORMAT_R8G8B8A8_UNORM),
            .normalTex = loadMaterialTexture(gltfMaterial.normalTexture.index, VK_FORMAT_R8G8B8A8_UNORM),
            .occlusionTex = loadMaterialTexture(gltfMaterial.occlusionTexture.index, VK_FORMAT_R8G8B8A8_UNORM),
        };

        if (gltfMaterial.pbrMetallicRoughness.baseColorFactor.size() >= 4) {
            for (size_t i = 0; i < gltfMaterial.pbrMetallicRoughness.baseColorFactor.size(); ++i)
                material.baseColor[i] = static_cast<float>(gltfMaterial.pbrMetallicRoughness.baseColorFactor[i]);
        }

        loadedMaterials[index] = storePBRMaterial(material, modelName + "_" + gltfMaterial.name);
    }

    // then load meshes and nodes 
    Model model{ .name = modelName };

    // each mesh is split into multiple meshes, one per primitive. keep track of loaded meshes with a separate map
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::map<int,std::vector<int>> loadedMeshes;

    for (size_t i = 0; i < gltfModel.meshes.size(); ++i)
        loadGLTFMesh(i, model, gltfModel, loadedMeshes, loadedMaterials, vertices, indices);

    const tinygltf::Scene &scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];

    for (size_t i = 0; i < scene.nodes.size(); i++)
        loadGLTFNode(-1, scene.nodes[i], gltfModel.nodes[scene.nodes[i]], model, gltfModel, loadedMeshes);

    // upload data to gpu and return model
    model.upload(vertices, indices, framesInFlightCount, device, allocator);

    return model;
}

void Scene::updateUniforms(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, uint32_t currentFrameIndex)
{
    struct {
        glm::mat4 projection;
        glm::mat4 view;
        glm::vec4 viewPos;
    } cameraData;

    cameraData.projection = camera.projection();
    cameraData.view = camera.view();
    cameraData.viewPos = { camera.position, 1.0f };

    cameraBuffers.update(currentFrameIndex, &cameraData, sizeof(cameraData));
    materialBuffers.update(currentFrameIndex, materials.data(), materials.dataSize());
    pbrMaterialBuffers.update(currentFrameIndex, pbrMaterials.data(), pbrMaterials.dataSize());

    uint32_t lightsCount = lights.size();
    lightBuffers.update(currentFrameIndex, &lightsCount, sizeof(uint32_t));
    lightBuffers.update(currentFrameIndex, lights.data(), sizeof(Light) * lights.size(), sizeof(uint32_t) * 4);

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, offsetof(BindlessPushConstants,camera),
        sizeof(VkDeviceAddress), &cameraBuffers.deviceAddress(currentFrameIndex));
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, offsetof(BindlessPushConstants,materials),
        sizeof(VkDeviceAddress), &pbrMaterialBuffers.deviceAddress(currentFrameIndex));
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, offsetof(BindlessPushConstants,lights),
        sizeof(VkDeviceAddress), &lightBuffers.deviceAddress(currentFrameIndex));
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, offsetof(BindlessPushConstants,lightClusters),
        sizeof(VkDeviceAddress), &lightClusterBuffers.deviceAddress(currentFrameIndex));
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_ALL, offsetof(BindlessPushConstants,lightClusterInfo),
        sizeof(VkDeviceAddress), &lightClusterInfoBuffers.deviceAddress(currentFrameIndex));
}

Model Scene::loadModel(std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices, const std::string &name)
{
    Model model{ .name = name };

    model.meshes.push_back({
        .indexCount = static_cast<uint32_t>(indices.size()),
        .vertexCount = static_cast<uint32_t>(vertices.size()),
        .volume = computeVolume(vertices)
    });

    model.nodes.push_back({
        .meshIndex = 0
    });

    model.upload(vertices, indices, framesInFlightCount, device, allocator);

    return model;
}
