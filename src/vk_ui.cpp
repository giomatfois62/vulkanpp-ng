#include "vk_ui.hpp"
#include "vk_utils.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"
#include "IconsFontAwesome4.h"

#include <iostream>
#include <filesystem>
#include <set>

using namespace vke;

template<class T>
void drawResourcesUI(T &res, Scene &scene)
{
    std::vector<const char*> names;
    static int currentIndex = 0;

    for (auto &pair : res.itemsMap)
        names.push_back(pair.first.c_str());

    ImGui::ListBox("Models", &currentIndex, names.data(), names.size());

    auto &item = res.get(names[currentIndex]);
    drawUI(item, scene);
}

void vke::drawUI(Scene &scene)
{
    if (ImGui::BeginTabBar("SceneTabBar")) {
        if (ImGui::BeginTabItem("Models")) {
            //DrawModels(scene, ui);
            if (ImGui::Button(ICON_FA_FOLDER " Load Model"))
                ImGui::OpenPopup("Select File:");
            std::string modelPath;
            if (ImGui::SelectFile("", modelPath, { ".obj", ".gltf", ".glb" })) {
                std::cout << "Selected Model: " << modelPath << std::endl;
                auto model = scene.loadModel(modelPath);
                model.instances.push_back({ .transform = glm::mat4(1.0f) });
                scene.storeModel(model);
            }

            drawResourcesUI(scene.models, scene);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Textures")) {
            drawResourcesUI(scene.textures, scene);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Materials")) {
            drawResourcesUI(scene.pbrMaterials, scene);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Lights")) {
            //ImGui::DrawCamera(scene->camera);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Camera")) {
            drawUI(scene.camera);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void vke::drawUI(Material &material, Scene &scene)
{
    ImGui::ColorEdit3("Ambient:", &material.ambient[0]);
    ImGui::ColorEdit3("Diffuse:", &material.diffuse[0]);
    ImGui::ColorEdit3("Specular:", &material.specular[0]);
    ImGui::SliderFloat("Shininess:", &material.shininess, 1, 1024);
    // TODO: textures
}

void vke::drawUI(PBRMaterial &material, Scene &scene)
{
    ImGui::SliderFloat4("Base Color:", &material.baseColor[0], 0.0f, 1.0f);
    ImGui::SliderFloat("Metallic Factor:", &material.metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness Factor:", &material.roughness, 0.0f, 1.0f);
    // TODO: textures
}

void vke::drawUI(Light &light)
{
    std::vector<const char*> lightTypes = { "Point", "Directional", "Spot" };
    int currentIndex = static_cast<int>(light.type);

    ImGui::Text("Light Type:"); ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##LightTypes", &currentIndex, lightTypes.data(), lightTypes.size());

    light.type = static_cast<LightType>(currentIndex);

    if (light.type == LightType::Directional || light.type == LightType::Spot) {
        ImGui::SliderFloat3("Light Direction:", &light.direction[0], -1, 1);
    }

    if (light.type != LightType::Directional) {
        ImGui::InputFloat3("Light Position: ", &light.position[0]);
        ImGui::InputFloat("Light Constant: ", &light.constant);
        ImGui::InputFloat("Light Linear: ", &light.linear);
        ImGui::InputFloat("Light Quadratic: ", &light.quadratic);
    }

    if (light.type == LightType::Spot) {
        ImGui::InputFloat("Light CutOff: ", &light.cutOff);
        ImGui::InputFloat("Light OuterCutOff: ", &light.outerCutOff);
    }

    ImGui::ColorEdit3("Light Ambient:", &light.ambient[0]);
    ImGui::ColorEdit3("Light Diffuse:", &light.diffuse[0]);
    ImGui::ColorEdit3("Light Specular:", &light.specular[0]);
}

void vke::drawUI(Model &model, Scene &scene)
{
    int current = 0;

    ImGui::Text("Meshes");
    for (auto &mesh : model.meshes) {
        char label[16];
        sprintf(label, "Mesh %d", current++);

        if (ImGui::TreeNode(label)) {
            ImGui::Text("Vertices: %u", mesh.vertexCount);
            ImGui::Text("Triangles: %u", mesh.indexCount/3);
            ImGui::InputFloat3("min", &mesh.volume.min[0]);
            ImGui::InputFloat3("max", &mesh.volume.max[0]);
            // TODO: material
            ImGui::TreePop();
        }
    }
    // TODO: node transforms
}

void vke::drawUI(Texture &texture, Scene &scene)
{

}

void vke::drawUI(Camera &camera)
{
    ImGui::SliderFloat("Speed", &camera.movementSpeed, 0, 100);
    ImGui::SliderFloat("Sensitivity", &camera.mouseSensitivity, 0, 1);
    ImGui::InputFloat3("Position", &camera.position[0]);
    ImGui::SliderFloat("NearPlane", &camera.nearPlane, 0.001, 500);
    ImGui::SliderFloat("FarPlane", &camera.farPlane, 0.001, 500);
}

bool ImGui::InputMat4(const char *id, glm::mat4 &mat)
{
    uint8_t modified = 0;

    const float width = ImGui::GetContentRegionAvail().x / 5.0f;
    const char* labels[4] = { "x", "y", "z", "w"};
    char label[16];

    ImGui::PushID(id);
    for (int row = 0; row < 4; ++row) {
        for (int i = 0; i < 3; ++i) {
            sprintf(label, "##x_%d_%d", row, i);

            ImGui::SetNextItemWidth(width);
            modified += ImGui::InputFloat(label, &mat[i][row]);
            ImGui::SameLine();
        }

        ImGui::SetNextItemWidth(width);
        ImGui::InputFloat(labels[row], &mat[3][row]);
    }

    ImGui::PopID();

    return (bool)modified;
}

bool ImGui::SelectFile(const std::string &initialPath, std::string &selectedFile, const std::vector<std::string> &filters)
{
    namespace fs = std::filesystem;

    static fs::path currentPath = initialPath.empty() ? fs::current_path() : fs::path(initialPath);

    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Select File:", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", currentPath.c_str());

        if (ImGui::Button("Parent Folder")) { currentPath = currentPath.parent_path(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }

        std::set<std::string> orderedFolders;
        std::set<std::string> orderedFiles;

        for (const auto &entry : fs::directory_iterator(currentPath)) {
            std::string filename = entry.path().filename().c_str();
            if (filename[0] == '.')
                continue;

            if (fs::is_directory(entry)) {
                orderedFolders.insert(entry.path().filename());
            } else {
                if (!filters.empty()) {
                    std::string fileExtension = entry.path().extension();
                    for (auto &ext : filters) {
                        if (ext == fileExtension)
                            orderedFiles.insert(entry.path().filename());
                    }
                } else {
                    orderedFiles.insert(entry.path().filename());
                }
            }
        }

        std::vector<std::string> filesInPath;
        std::vector<std::string> filesInPathWithIcons;
        std::vector<const char*> filesInPathCStrings;

        for (auto &dir : orderedFolders) {
            filesInPath.push_back(dir);
            filesInPathWithIcons.push_back(std::string(ICON_FA_FOLDER " ") + dir);
        }

        for (auto &file : orderedFiles) {
            filesInPath.push_back(file);
            filesInPathWithIcons.push_back(std::string(ICON_FA_FILE " ") + file);
        }

        for (const auto &entry : filesInPathWithIcons)
            filesInPathCStrings.push_back(entry.c_str());

        static int selectedIndex = 0;

        if (ImGui::ListBox("##Files", &selectedIndex, filesInPathCStrings.data(), filesInPathCStrings.size(), 10)) {
            fs::path selectedPath = currentPath / filesInPath[selectedIndex];

            if (fs::is_directory(selectedPath)) {
                currentPath = selectedPath;
            } else {
                selectedFile = selectedPath.c_str();
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return true;
            }
        }
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }

    return false;
}

void UI::init(VkInstance instance, VkDevice device, VkPhysicalDevice gpu, VkQueue queue, SDL_Window *window, VkFormat imageFormat, uint32_t imageCount)
{
    this->device = device;

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

    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &imguiDescriptorPool));

    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(window);

    //dynamic rendering parameters
    VkPipelineRenderingCreateInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &imageFormat,
    };

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = instance,
        .PhysicalDevice = gpu,
        .Device = device,
        .Queue = queue,
        .DescriptorPool = imguiDescriptorPool,
        .MinImageCount = 2,
        .ImageCount = imageCount,
        .PipelineInfoMain = { .MSAASamples = VK_SAMPLE_COUNT_1_BIT, .PipelineRenderingCreateInfo = renderInfo },
        .UseDynamicRendering = true
    };

    ImGui_ImplVulkan_Init(&initInfo);

    // merge icons from Font Awesome
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    float baseFontSize = 13.0f; // 13.0f is the size of the default font. Change to the font size you use.
    float iconFontSize = baseFontSize * 2.0f / 3.0f; // FontAwesome fonts need to have their sizes reduced by 2.0f/3.0f in order to align correctly

    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = iconFontSize;
    io.Fonts->AddFontFromFileTTF("res/fonts/" FONT_ICON_FILE_NAME_FA, iconFontSize, &icons_config, icons_ranges);
}

void UI::cleanup()
{
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
}

void UI::update(std::function<void ()> drawCommands)
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    drawCommands();

    ImGui::Render();
}

void UI::render(VkCommandBuffer cmd, VkImageView targetImageView, VkExtent2D targetImageExtent)
{
    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = targetImageView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        //.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, //VK_ATTACHMENT_STORE_OP_STORE,
    };

    VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = { .extent = targetImageExtent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo
    };

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void UI::processEvent(SDL_Event &e)
{
    ImGui_ImplSDL2_ProcessEvent(&e);
}
