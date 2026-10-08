cmake_minimum_required(VERSION 3.20)

project(RealtimeRenderer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- GLFW ----------------

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glfw)

# ---------------- GLAD ----------------

set(GLAD_SOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/glad")
add_subdirectory("${GLAD_SOURCES_DIR}/cmake" glad_cmake)

glad_add_library(
    glad_gl_core_45
    STATIC
    REPRODUCIBLE
    API gl:core=4.5
)

# ---------------- GLM ----------------

add_subdirectory(external/glm)

# This checkout contains GLM's header directory, not its repository root.
# Export its parent so consumers can resolve <glm/glm.hpp> via glm::glm.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glm/glm.hpp")
    set_property(TARGET glm-header-only PROPERTY INTERFACE_INCLUDE_DIRECTORIES
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/external>"
    )
endif()

# ---------------- Assimp ----------------

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(external/assimp)

# ---------------- simdjson ----------------

set(SIMDJSON_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(SIMDJSON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SIMDJSON_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/simdjson)

# ---------------- fastgltf ----------------

set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(external/fastgltf)

# ---------------- KTX-Software ----------------

set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_GL_UPLOAD ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_VK_UPLOAD OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_DOC OFF CACHE BOOL "" FORCE)
set(KTX_GIT_VERSION_FULL v4.3.2 CACHE STRING "" FORCE)

add_subdirectory(external/KTX-Software)

# ---------------- Dear ImGui ----------------

add_library(
    imgui
    STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(
    imgui
    PUBLIC
    ${CMAKE_SOURCE_DIR}/external/imgui
    ${CMAKE_SOURCE_DIR}/external/imgui/backends
)

target_link_libraries(
    imgui
    PUBLIC
    glfw
)

# ---------------- Renderer ------------------

add_executable(
    renderer
    src/main.cpp
    src/Shader.cpp
    src/Texture.cpp
    src/Camera.cpp
    src/Material.cpp
    src/Mesh.cpp
    src/Model.cpp
    src/GltfLoader.cpp
    src/Transform.cpp
    src/SceneObject.cpp
    src/Scene.cpp
    src/AssetManager.cpp
    src/Renderer.cpp
    src/EnvironmentMap.cpp
    
    external/ImGuizmo/ImGuizmo.cpp
)

target_include_directories(
    renderer
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/stb
    ${CMAKE_SOURCE_DIR}/external/ImGuizmo
)

target_link_libraries(
    renderer
    PRIVATE
    glfw
    glad_gl_core_45
    glm::glm
    assimp::assimp
    fastgltf::fastgltf
    ktx
    imgui
    comdlg32
)

# ---------------- Shaders ----------------

add_custom_target(renderer_shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/shaders"
    "$<TARGET_FILE_DIR:renderer>/shaders"
    COMMENT "Synchronizing renderer shaders"
    VERBATIM
)

add_dependencies(renderer renderer_shaders)

# ---------------- Assets ----------------

add_custom_command(
    TARGET renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:renderer>/assets"
    VERBATIM
)