cmake_minimum_required(VERSION 3.20)

project(RealtimeRenderer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---------------- GLFW ----------------

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glfw)

# ---------------- GLAD ----------------

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    SOURCE_SUBDIR cmake
)

FetchContent_MakeAvailable(glad)

glad_add_library(
    glad_gl_core_33
    STATIC
    REPRODUCIBLE
    API gl:core=3.3
)

# ---------------- GLM ----------------

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

FetchContent_MakeAvailable(glm)

# ---------------- Assimp ----------------
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(assimp)

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

# ---------------- Renderer ----------------

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
)

target_include_directories(
    renderer
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/stb
)

target_link_libraries(
    renderer
    PRIVATE
    glfw
    glad_gl_core_33
    glm::glm
    assimp::assimp
    fastgltf::fastgltf
    ktx
)

# Shader edits and renames do not relink renderer, so copy them on every build.
add_custom_target(renderer_shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/shaders"
    "$<TARGET_FILE_DIR:renderer>/shaders"
    COMMENT "Synchronizing renderer shaders"
    VERBATIM
)
add_dependencies(renderer renderer_shaders)

add_custom_command(
    TARGET renderer POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:renderer>/assets"
    VERBATIM
)