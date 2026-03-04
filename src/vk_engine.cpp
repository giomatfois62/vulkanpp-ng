#include "vk_engine.hpp"
#include "vk_buffer.hpp"
#include "vk_utils.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include <SDL_vulkan.h>

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace std {
    template<> struct hash<vke::Vertex> {
        size_t operator()(vke::Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
               (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
               (hash<glm::vec2>()(vertex.uv) << 1) ^
               (hash<uint32_t>()(vertex.material) << 1);
        }
    };
}

using namespace std;
using namespace vke;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

// https://henriquegois.dev/posts/bindless-resources-in-vulkan/
// Select a binding for each descriptor type
constexpr int STORAGE_BINDING = 0;
constexpr int SAMPLER_BINDING = 1;
constexpr int IMAGE_BINDING = 2;

// Max count of each descriptor type
// You can query the max values for these with
// physicalDevice.getProperties().limits.maxDescriptrorSet*******
constexpr int STORAGE_COUNT = 65536;
constexpr int SAMPLER_COUNT = 65536;
constexpr int IMAGE_COUNT = 65536;

Engine::Engine(int, char**)
{

}

void Engine::run()
{
	init();
    loop();
	cleanup();
}

void Engine::quit()
{
	shouldQuit = true;
}

void Engine::setWindowTitle(const char *title)
{
	windowTitle = title;

	if (window)
		SDL_SetWindowTitle(window, title);
}

void Engine::setApplicationName(const char *name)
{
    vulkan.applicationName = name;
}

void Engine::init()
{
	createWindow();
    createVulkan();
    createSwapchain();
    createDrawImage();
    createColorResources();
    createDepthResources();
    //createDefaultRenderPass();
    //createFrameBuffers();
    createFrameObjects();
    createBindlessDescriptors();
    initImGui();

	onInit();
}

void Engine::loop()
{
	shouldQuit = false;

	auto start = std::chrono::high_resolution_clock::now();
	auto last = start;
	size_t frameCounter = 0;

	while (!shouldQuit) {
		auto end = std::chrono::high_resolution_clock::now();
		lagInMillisecs = std::chrono::duration<float, std::milli>(end - start).count();
		start = end;

		float dt = lagInMillisecs * 0.001f;
		float fpsTimer = std::chrono::duration<float,std::milli>(end - last).count();

		if (fpsTimer > 1000.0f) {
			uint32_t fps = static_cast<uint32_t>(frameCounter * (1000.0f / fpsTimer));

            std::string title = vulkan.applicationName + std::string(" - ") + std::to_string(fps) + "fps";
			setWindowTitle(title.c_str());

			frameCounter = 0;
			last = start;
		}

		processEvents();

		update(dt);

        if (!isHidden) {
            updateImGui();
            renderFrame();
        }

        frameCounter++;
	}

	// wait for pending operations
	waitIdle();
}

void Engine::cleanup()
{
	onCleanup();

    for (auto &texture : assets.textures.items) {
        texture.cleanup(vulkan.device, vulkan.allocator);
    }

    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(vulkan.device, imguiDescriptorPool, nullptr);

    vkDestroyDescriptorSetLayout(vulkan.device, bindlessDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(vulkan.device, descriptorPool, nullptr);

    for (auto &frame : framesInFlight) {
        vkDestroySemaphore(vulkan.device, frame.imageAvailableSemaphore, nullptr);
        vkDestroyFence(vulkan.device, frame.renderFinishedFence, nullptr);

        vkFreeCommandBuffers(vulkan.device, frame.commandPool, 1, &frame.mainCommandBuffer);
        vkDestroyCommandPool(vulkan.device, frame.commandPool, nullptr);
	}

    for (auto &semaphore: renderFinishedSemaphores) {
        vkDestroySemaphore(vulkan.device, semaphore, nullptr);
    }

    //for (auto &frameBuffer : frameBuffers)
    //    vkDestroyFramebuffer(vulkan.device, frameBuffer, nullptr);

    //vkDestroyRenderPass(vulkan.device, renderPass, nullptr);

    cleanupRenderingResources();

    swapchain.cleanup();

    vulkan.cleanup();

	SDL_DestroyWindow(window);
	window = nullptr;
}

void Engine::processEvents()
{
    SDL_Event e;

	while (SDL_PollEvent(&e) != 0) {
		if (e.type == SDL_QUIT)
			quit();

		if (e.type == SDL_WINDOWEVENT) {
			if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
				int width = e.window.data1;
				int height = e.window.data2;

                resize(width, height);
			}

            if (e.window.event == SDL_WINDOWEVENT_MINIMIZED || e.window.event == SDL_WINDOWEVENT_HIDDEN) {
                isHidden = true;
            }

            if (e.window.event == SDL_WINDOWEVENT_RESTORED || e.window.event == SDL_WINDOWEVENT_SHOWN) {
                isHidden = false;
            }
		}

		if (e.type == SDL_KEYDOWN) {
			if (e.key.keysym.sym == SDLK_f) {
				float dt = lagInMillisecs * 0.001f;
				float fps = 1.0f / dt;

				cout << "FPS: " << fps << endl;
			}
		}

        benchmarks.imguiEventsTime = measureExecution<std::chrono::microseconds>([&]{
            ImGui_ImplSDL2_ProcessEvent(&e);
        });

        // return if io.WantCaptureMouse or io.WantCaptureKeyboard is true
        if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && ImGui::GetIO().WantCaptureKeyboard)
            return;

        if ((e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEWHEEL || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) &&
            ImGui::GetIO().WantCaptureMouse)
            return;

        benchmarks.appEventsTime = measureExecution<std::chrono::microseconds>([&]{
            processEvent(e);
        });

        //SDL_WaitEventTimeout(nullptr, 10);
	}
}

void Engine::resize(int width, int height)
{
	windowSize.width = width;
	windowSize.height = height;

	recreateSwapchain();

	onResize(width, height);
}

Texture Engine::createTexture(const TextureData &data)
{
    Texture texture;

    Buffer uploadbuffer = createStagingBuffer(data.imageSize, vulkan.allocator);
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
        vulkan.allocator
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

    VK_CHECK(vkCreateImageView(vulkan.device, &viewInfo, nullptr, &texture.image.view));

    VkCommandBuffer cmd = vulkan.createCommandBuffer(true);

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

    vulkan.submitCommandBuffer(cmd, vulkan.graphicsQueue, true);

    vmaDestroyBuffer(vulkan.allocator, uploadbuffer.handle, uploadbuffer.allocation);

    texture.sampler = createDefaultSampler(vulkan.device);

    return texture;
}

uint32_t Engine::loadTexture(const std::string &path, VkFormat format)
{
    TextureData data = loadTextureData(path, format);

    return storeTexture(createTexture(data));
}

uint32_t Engine::storeTexture(Texture texture)
{
    uint32_t textureID = assets.textures.insert(texture);

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

    vkUpdateDescriptorSets(vulkan.device, 1, &write, 0, nullptr);

    return textureID;
}

uint32_t Engine::storeMaterial(Material material)
{
    return assets.materials.insert(material);
}

Model Engine::loadModel(const std::string &path)
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

Model Engine::loadOBJ(const std::string &path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string baseDir = getFileDirectory(path);

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

        loadedMaterials.push_back(storeMaterial(material));
    }

    std::vector<Mesh> meshes;

    for (auto &shape : shapes) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<Vertex, uint32_t> uniqueVertices;

        size_t currentIndex = 0;
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

            uint32_t material = shape.mesh.material_ids[currentIndex/3];
            v.material = loadedMaterials.size() ? loadedMaterials[material] : 0;

            if (uniqueVertices.count(v) == 0) {
                uniqueVertices[v] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(v);
            }

            indices.push_back(uniqueVertices[v]);
            currentIndex++;
        }

        Mesh mesh(vertices, indices);

        // set mesh material if any per-face material is defined
        mesh.material = shape.mesh.material_ids.size() ? loadedMaterials[shape.mesh.material_ids[0]] : 0;

        meshes.push_back(mesh);
    }

    return Model(meshes);
}

Model Engine::loadGLTF(const std::string &path)
{
    // TODO: load materials and textures
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    bool res;

    if (getFileExtension(path) == "glb") {
        res = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    } else {
        res = loader.LoadASCIIFromFile(&model, &err, &warn, path);
    }

    if (!warn.empty()) {
        std::cout << "GLTF Warning: " << warn << std::endl;
    }

    if (!err.empty() || !res) {
        std::cerr << "GLTF Error: " << err << std::endl;
        throw std::runtime_error("Failed to load glTF model from " + path);
    }

    std::vector<Mesh> meshes;
    std::vector<uint32_t> loadedMaterials;
    std::map<std::string,uint32_t> loadedTextures;

    auto loadGltfTexture = [&](uint32_t textureIndex, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB) {
        if (textureIndex < 0)
            return 0;

        auto gltfTexture = model.textures[textureIndex];
        auto imageIndex = gltfTexture.source;
        auto image = model.images[imageIndex];

        return 0;
    };

    for (const auto& gltfMat : model.materials) {
        PBRMaterial mat {
            .baseColor = {
                static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[0]),
                static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[1]),
                static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[2]),
                static_cast<float>(gltfMat.pbrMetallicRoughness.baseColorFactor[3]),
            },
            .metallic = static_cast<float>(gltfMat.pbrMetallicRoughness.metallicFactor),
            .roughness = static_cast<float>(gltfMat.pbrMetallicRoughness.roughnessFactor),
            // TODO: assign textures
        };
        // TODO: store material
    }

    for (const auto& mesh : model.meshes) {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        for (const auto& primitive : mesh.primitives) {
            size_t vertexCount = model.accessors[primitive.attributes.at("POSITION")].count;
            uint32_t baseVertexIndex = static_cast<uint32_t>(vertices.size());

            auto getBuffer = [&](const std::string &type){
                const float *buf = nullptr;

                if (primitive.attributes.find(type) != primitive.attributes.end()) {
                    const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at(type)];
                    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                    buf = reinterpret_cast<const float*>(&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
                }

                return buf;
            };

            // get primitive vertices
            const float *posBuffer = getBuffer("POSITION");
            const float *normBuffer = getBuffer("NORMAL");
            const float *uvBuffer = getBuffer("TEXCOORD_0");

            for (size_t i = 0; i < vertexCount; ++i) {
                Vertex v {
                    .pos = { posBuffer[i*3], posBuffer[i*3+1], posBuffer[i*3+2] },
                    .color = { 1.0f, 1.0f, 1.0f }
                };

                if (normBuffer)
                    v.normal = { normBuffer[i*3], normBuffer[i*3+1], normBuffer[i*3+2] };

                if (uvBuffer)
                    v.uv = { uvBuffer[i*2], 1.0f - uvBuffer[i*2+1] };

                vertices.push_back(v);
            }

            // get primitive indices
            const tinygltf::Accessor &indexAccessor = model.accessors[primitive.indices];
            const tinygltf::BufferView &indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const tinygltf::Buffer &indexBuffer = model.buffers[indexBufferView.buffer];

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
        }

        Mesh _mesh(vertices, indices);
        meshes.push_back(_mesh);
    }

    return Model(meshes);
}

void Engine::renderFrame()
{
    Frame currentFrame = framesInFlight[currentFrameIndex];

	if (!prepareFrame(currentFrame))
		return;

    VkCommandBuffer cmd = currentFrame.mainCommandBuffer;

    beginDraw(cmd);
    draw(cmd);
    drawImGui(cmd);
    endDraw(cmd);

	presentFrame(currentFrame);

	currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Engine::beginDraw(VkCommandBuffer cmd)
{
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo cmdBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    changeImageLayout(cmd, drawImage.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // vkguide?
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );

    changeImageLayout(cmd, colorImage.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );

    changeImageLayout(cmd, depthImage.handle,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, .levelCount = 1, .layerCount = 1 }
    );
}

void Engine::endDraw(VkCommandBuffer cmd)
{
    changeImageLayout(cmd, swapchain.images[currentImageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    );

    VK_CHECK(vkEndCommandBuffer(cmd));
}

void Engine::waitIdle()
{
    vkDeviceWaitIdle(vulkan.device);
}

void Engine::onInit()
{

}

void Engine::onCleanup()
{

}

void Engine::onResize(int, int)
{

}

void Engine::draw(VkCommandBuffer)
{

}

void Engine::drawUI()
{

}

void Engine::requestGPUFeatures(GPUFeatures &)
{

}

VkExtent2D Engine::drawExtent()
{
    return {
        static_cast<uint32_t>(swapchain.extent.width * renderScale),
        static_cast<uint32_t>(swapchain.extent.height * renderScale)
    };
}

void Engine::createRenderingResources()
{
    createDrawImage();
    createColorResources();
    createDepthResources();
}

void Engine::cleanupRenderingResources()
{
    vkDestroyImageView(vulkan.device, depthImage.view, nullptr);
    vmaDestroyImage(vulkan.allocator, depthImage.handle, depthImage.allocation);

    vkDestroyImageView(vulkan.device, colorImage.view, nullptr);
    vmaDestroyImage(vulkan.allocator, colorImage.handle, colorImage.allocation);

    vkDestroyImageView(vulkan.device, drawImage.view, nullptr);
    vmaDestroyImage(vulkan.allocator, drawImage.handle, drawImage.allocation);
}

bool Engine::msaaEnabled()
{
    return MSAASamples != VK_SAMPLE_COUNT_1_BIT;
}

void Engine::update(float)
{

}

void Engine::processEvent(const SDL_Event&)
{

}

void Engine::createWindow()
{
	SDL_Init(SDL_INIT_VIDEO);

    uint32_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN;

	window = SDL_CreateWindow(
		windowTitle, //window title
		SDL_WINDOWPOS_UNDEFINED, //window position x (don't care)
		SDL_WINDOWPOS_UNDEFINED, //window position y (don't care)
		windowSize.width,  //window width in pixels
		windowSize.height, //window height in pixels
		flags
	);

	if (!window)
		throw std::runtime_error("Failed creating SDL2 window.");

	int width, height;
	SDL_GetWindowSize(window, &width, &height);

	windowSize.width = width;
	windowSize.height = height;
}

void Engine::createVulkan()
{
    requestGPUFeatures(vulkan.requestedFeatures);

    vulkan.requestedFeatures.vk10 = {
        .sampleRateShading = VK_TRUE,
        .fillModeNonSolid = VK_TRUE, // for wireframe rendering
        .samplerAnisotropy = VK_TRUE,
    };

    vulkan.requestedFeatures.vk12 = {
        .descriptorIndexing = VK_TRUE,
        // https://henriquegois.dev/posts/bindless-resources-in-vulkan/
        .descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
    };

    vulkan.requestedFeatures.vk13 = {
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };

    vulkan.create(window);
}

void Engine::createSwapchain()
{
    swapchain.init(vulkan.gpu, vulkan.surface, vulkan.device);
	swapchain.create(windowSize.width, windowSize.height);
}

void Engine::createDrawImage()
{
    auto extent = drawExtent();

    drawImage = createImage(
        VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = swapchain.imageFormat,
            .extent = { extent.width, extent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        },
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        },
        vulkan.allocator
    );

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = drawImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain.imageFormat,
        .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };

    VK_CHECK(vkCreateImageView(vulkan.device, &viewInfo, nullptr, &drawImage.view));
}

void Engine::createColorResources()
{
    auto extent = drawExtent();

    colorImage = createImage(
        VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = swapchain.imageFormat,
            .extent = { extent.width, extent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = MSAASamples,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        },
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        },
        vulkan.allocator
    );

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = colorImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = swapchain.imageFormat,
        .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
    };

    VK_CHECK(vkCreateImageView(vulkan.device, &viewInfo, nullptr, &colorImage.view));
}

void Engine::createDepthResources()
{
    VkFormat format = findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,
        vulkan.gpu
    );

    auto extent = drawExtent();

    depthImage = createImage(
        VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = { extent.width, extent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = MSAASamples,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        },
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        },
        vulkan.allocator
    );

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthImage.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        //VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        .subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 }
    };

    VK_CHECK(vkCreateImageView(vulkan.device, &viewInfo, nullptr, &depthImage.view));
}

void Engine::recreateSwapchain()
{
    waitIdle(); // wait for pending operations

    swapchain.create(windowSize.width, windowSize.height);

    cleanupRenderingResources();
    createRenderingResources();

    //vkDestroyRenderPass(vulkan.device, renderPass, nullptr);
    //createDefaultRenderPass();

    // rebuild objects with new swapchain
    //for (auto &frameBuffer : frameBuffers)
    //    vkDestroyFramebuffer(vulkan.device, frameBuffer, nullptr);
    //createFrameBuffers();

    waitIdle(); // wait for pending init
}

void Engine::updateImGui()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    drawUI();

    ImGui::Render();
}

void Engine::drawImGui(VkCommandBuffer cmd)
{
    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain.imageViews[currentImageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        //.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        //.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, //VK_ATTACHMENT_STORE_OP_STORE,
    };

    VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .extent = windowSize },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo
    };

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void Engine::createDefaultRenderPass()
{
    VkAttachmentDescription colorAttachment{
        .format = swapchain.imageFormat,
        .samples = MSAASamples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = msaaEnabled() ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentRef{
        // attachment number will index into the pAttachments array in the parent renderpass itself
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentDescription depthAttachment{
        .format = depthImage.info.format,
        .samples = MSAASamples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depthAttachmentRef{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentDescription colorAttachmentResolve{
        .format = swapchain.imageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorAttachmentResolveRef{
        .attachment = 2,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    // we are going to create 1 subpass, which is the minimum you can do
    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pResolveAttachments = msaaEnabled() ? &colorAttachmentResolveRef : nullptr,
        .pDepthStencilAttachment = &depthAttachmentRef
    };

    VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    };

    std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };

    if (msaaEnabled()) {
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        attachments.push_back(colorAttachmentResolve);
    }

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VK_CHECK(vkCreateRenderPass(vulkan.device, &renderPassInfo, nullptr, &renderPass));
}

void Engine::createFrameBuffers()
{
    // create the framebuffers for the swapchain images.
    // this will connect the render-pass to the images for rendering
    VkFramebufferCreateInfo frameBufferInfo{
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .width = swapchain.extent.width,
        .height = swapchain.extent.height,
        .layers = 1
    };

    // grab how many images we have in the swapchain
    const uint32_t imageCount = swapchain.images.size();

    frameBuffers = std::vector<VkFramebuffer>(imageCount);

    // create framebuffers for each of the swapchain image views
    for (uint32_t i = 0; i < imageCount; i++) {
        std::vector<VkImageView> attachments;

        if (msaaEnabled()) {
            attachments = { colorImage.view, depthImage.view, swapchain.imageViews[i] };
        } else {
            attachments = { swapchain.imageViews[i], depthImage.view };
        }

        frameBufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size()),
        frameBufferInfo.pAttachments = attachments.data();

        VK_CHECK(vkCreateFramebuffer(vulkan.device, &frameBufferInfo, nullptr, &frameBuffers[i]));
    }
}

void Engine::createFrameObjects()
{
	framesInFlight.resize(MAX_FRAMES_IN_FLIGHT);

    uint32_t queueFamilyIndex = vulkan.queueFamilies.graphics.value();

    VkCommandPoolCreateInfo commandPoolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };

    VkSemaphoreCreateInfo semaphoreInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

	for (auto &frame : framesInFlight) {
        VK_CHECK(vkCreateCommandPool(vulkan.device, &commandPoolInfo, nullptr, &frame.commandPool));

        VkCommandBufferAllocateInfo cmdAllocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = frame.commandPool,
            .commandBufferCount = 1
        };

        VK_CHECK(vkAllocateCommandBuffers(vulkan.device, &cmdAllocInfo, &frame.mainCommandBuffer));

        VK_CHECK(vkCreateSemaphore(vulkan.device, &semaphoreInfo, nullptr,
            &frame.imageAvailableSemaphore));

        VK_CHECK(vkCreateFence(vulkan.device, &fenceInfo, nullptr,
			&frame.renderFinishedFence));
	}

    renderFinishedSemaphores.resize(swapchain.images.size());

    for (size_t i = 0; i < swapchain.images.size(); ++i) {
        VkSemaphore renderSemaphore;

        VK_CHECK(vkCreateSemaphore(vulkan.device, &semaphoreInfo, nullptr, &renderSemaphore));

        renderFinishedSemaphores[i] = renderSemaphore;
    }
}

void Engine::createBindlessDescriptors()
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

    VK_CHECK(vkCreateDescriptorPool(vulkan.device, &poolCreateInfo, nullptr, &descriptorPool));

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

    VK_CHECK(vkCreateDescriptorSetLayout(vulkan.device, &setLayoutCreateInfo, nullptr, &bindlessDescriptorSetLayout));

    std::vector<VkDescriptorSetLayout> sets = { bindlessDescriptorSetLayout };

    VkDescriptorSetAllocateInfo setAllocateInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = sets.data()
    };

    VK_CHECK(vkAllocateDescriptorSets(vulkan.device, &setAllocateInfo, &bindlessDescriptorSet));
}

void Engine::initImGui()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
         { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
    };

    uint32_t maxSets = 0;
    for (VkDescriptorPoolSize& poolSize : poolSizes)
        maxSets += poolSize.descriptorCount;

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = maxSets,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VK_CHECK(vkCreateDescriptorPool(vulkan.device, &poolInfo, nullptr, &imguiDescriptorPool));

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(window);

    //dynamic rendering parameters
    VkPipelineRenderingCreateInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchain.imageFormat,
    };

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = vulkan.instance,
        .PhysicalDevice = vulkan.gpu,
        .Device = vulkan.device,
        .Queue = vulkan.graphicsQueue,
        .DescriptorPool = imguiDescriptorPool,
        .MinImageCount = 2,
        .ImageCount = static_cast<uint32_t>(swapchain.images.size()),
        .PipelineInfoMain = { .MSAASamples = VK_SAMPLE_COUNT_1_BIT, .PipelineRenderingCreateInfo = renderInfo },
        .UseDynamicRendering = true
    };

    ImGui_ImplVulkan_Init(&initInfo);
}

bool Engine::prepareFrame(Frame &frame)
{
	// acquire image from swapchain
    vkWaitForFences(vulkan.device, 1, &frame.renderFinishedFence, VK_TRUE, UINT64_MAX);

	VkResult res = swapchain.acquireNextImage(frame.imageAvailableSemaphore, &currentImageIndex);

    vkResetFences(vulkan.device, 1, &frame.renderFinishedFence);

	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		int width, height;
		SDL_GetWindowSize(window, &width, &height);

		resize(width, height);

		return false;
	} else {
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
			throw std::runtime_error("Failed acquiring image from swapchain.");

		return true;
	}
}

void Engine::presentFrame(Frame &frame)
{
    // Pipeline stages used to wait at for graphics queue submissions
	VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	// submit draw commands in mainbuffer
    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame.imageAvailableSemaphore,
        .pWaitDstStageMask = &waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame.mainCommandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphores[currentImageIndex]
    };

    vkResetFences(vulkan.device, 1, &frame.renderFinishedFence);

    VK_CHECK(vkQueueSubmit(vulkan.graphicsQueue, 1, &submit, frame.renderFinishedFence));

	// present frame to swapchain
    VkResult res = swapchain.presentImage(vulkan.presentQueue, currentImageIndex, renderFinishedSemaphores[currentImageIndex]);

	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		int width, height;
		SDL_GetWindowSize(window, &width, &height);

		resize(width, height);
	} else {
		if (res != VK_SUCCESS)
			throw std::runtime_error("Failed presenting image to swapchain.");
	}
}
