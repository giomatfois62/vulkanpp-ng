#ifndef VK_UI_HPP
#define VK_UI_HPP

#include "vk_camera.hpp"
#include "vk_light.hpp"
#include "vk_mesh.hpp"
#include "vk_scene.hpp"

namespace ImGui {

bool InputMat4(const char* id, glm::mat4 &mat);
bool SelectFolder(const std::string &initialPath, std::string &selected, bool showHidden = false);
bool SelectFile(const std::string &initialPath, std::string &selectedFile,
    bool showHidden = false, const std::vector<std::string> &filters = {});

} // end namespace ImGui

namespace vke {

void drawUI(Scene &scene);
void drawUI(Camera &camera);
void drawUI(Material &material, Scene &scene);
void drawUI(PBRMaterial &material, Scene &scene);
void drawUI(Light &light);
void drawUI(Model &model, Scene &scene);

} // end namespace vke

#endif // VK_UI_HPP
