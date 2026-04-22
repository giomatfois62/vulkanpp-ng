#ifndef VK_UI_HPP
#define VK_UI_HPP

#include "vk_camera.hpp"
#include "vk_light.hpp"
#include "vk_mesh.hpp"
#include "vk_scene.hpp"

#include <SDL_events.h>

namespace ImGui {

bool InputMat4(const char* id, glm::mat4 &mat);
bool SelectFile(const std::string &initialPath, std::string &selectedFile, const std::vector<std::string> &filters = {});

} // end namespace ImGui

namespace vke {

void drawUI(Scene &scene);
void drawUI(Camera &camera);
void drawUI(Material &material, Scene &scene);
void drawUI(PBRMaterial &material, Scene &scene);
void drawUI(Texture &texture, Scene &scene);
void drawUI(Light &light);
void drawUI(Model &model, Scene &scene);

class UI {
public:
    void init(VkInstance instance, VkDevice device, VkPhysicalDevice gpu, VkQueue queue,
        SDL_Window *window, VkFormat imageFormat, uint32_t imageCount);
    void cleanup();
    void update(std::function<void(void)> drawCommands);
    void render(VkCommandBuffer cmd, VkImageView targetImageView, VkExtent2D targetImageExtent);
    void processEvent(SDL_Event &e);

protected:
    VkDevice device;
    VkDescriptorPool imguiDescriptorPool;
};

} // end namespace vke

#endif // VK_UI_HPP
