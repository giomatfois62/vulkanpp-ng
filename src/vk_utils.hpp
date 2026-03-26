#ifndef VULKANUTILS_HPP
#define VULKANUTILS_HPP

#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <stdexcept>

template<typename Resolution, typename F, typename... Args>
double measureExecution(F func, Args&&... args){
    auto t1 = std::chrono::high_resolution_clock::now();
        func(std::forward<Args>(args)...);
    auto t2 = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<Resolution>(t2 - t1).count();
}

#define VK_CHECK(f) \
{ \
    VkResult res = (f);	\
    if (res != VK_SUCCESS) { \
        std::stringstream error; \
        error << "Fatal : " << #f << " returned \"" << \
        vke::errorString(res) << "\" in " << __FILE__ << \
        " at line " << __LINE__ << "\n"; \
        throw std::runtime_error(error.str()); \
    } \
}

#define CHECK(f) \
{ \
    bool res = (f);	\
    if (res != true) { \
        std::stringstream error; \
        error << "Fatal : " << #f << " returned false in " << \
        __FILE__ << " at line " << __LINE__ << "\n"; \
        throw std::runtime_error(error.str()); \
    } \
}

namespace vke {

std::vector<char> readFile(const std::string &filename);
std::string getFileExtension(const std::string &filename);
std::string getFileDirectory(const std::string &filename);
std::string getFileName(const std::string &filename);

std::string errorString(VkResult errorCode);

} // end namespace vke

#endif // VULKANUTILS_HPP
