cmake_minimum_required(VERSION 3.7)

project(vulkanpp)

# build flags
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-missing-field-initializers -pthread -fopenmp")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# dependencies
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})

# external includes
include_directories(external/imgui)
include_directories(external/IconFontCppHeaders)
include_directories(external/ktx/include)
include_directories(external/ktx/other_include)
include_directories(external/tinygltf)
include_directories(external/tinyobj)
include_directories(external/vma)

# imgui library
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
set(IMGUI_SRC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/imgui_stdlib.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp)
add_library(imgui STATIC ${IMGUI_SRC})

# ktx library
set(KTX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/ktx)
set(KTX_SRC
    ${KTX_DIR}/lib/texture.c
    ${KTX_DIR}/lib/hashlist.c
    ${KTX_DIR}/lib/checkheader.c
    ${KTX_DIR}/lib/swap.c
    ${KTX_DIR}/lib/memstream.c
    ${KTX_DIR}/lib/filestream.c
    ${KTX_DIR}/lib/vkloader.c)
add_library(ktx STATIC ${KTX_SRC})

# build project executable
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SRC
${SRC_DIR}/application.cpp
${SRC_DIR}/main.cpp
${SRC_DIR}/vk_buffer.cpp
${SRC_DIR}/vk_camera.cpp
${SRC_DIR}/vk_core.cpp
${SRC_DIR}/vk_engine.cpp
${SRC_DIR}/vk_geometry.cpp
${SRC_DIR}/vk_image.cpp
${SRC_DIR}/vk_light.cpp
${SRC_DIR}/vk_mesh.cpp
${SRC_DIR}/vk_pipeline.cpp
${SRC_DIR}/vk_scene.cpp
${SRC_DIR}/vk_thread.cpp
${SRC_DIR}/vk_ui.cpp
${SRC_DIR}/vk_utils.cpp
${SRC_DIR}/vk_volume.cpp)
add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES} ${Vulkan_LIBRARIES} ktx imgui)

# symlink resources after build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_SOURCE_DIR}/res $<TARGET_FILE_DIR:${PROJECT_NAME}>/res)
