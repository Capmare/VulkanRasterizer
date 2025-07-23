cmake_minimum_required(VERSION 3.20)
project(VulkanRasterizer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /Zi /Ob0 /Od")
    set(CMAKE_LINKER_FLAGS_DEBUG "/DEBUG")
endif()

if(NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR "VULKAN_SDK environment variable is not set. Please install and source the Vulkan SDK.")
endif()

set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

# === EXECUTABLE SOURCES ===
file(GLOB_RECURSE APP_SOURCES "src/*.cpp" "src/Factories/*.cpp")
add_executable(${PROJECT_NAME} ${APP_SOURCES})

# === SHADER COMPILATION USING .BAT ===
add_custom_target(compileShaders ALL
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning old SPIR-V files..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${SHADER_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E echo "Running compile.bat..."
        COMMAND ${CMAKE_COMMAND} -E chdir "${SHADER_DIR}" compile.bat
        COMMENT "Compiling shaders via compile.bat"
)

# Make main project depend on shader compilation
add_dependencies(${PROJECT_NAME} compileShaders)

# === COPY TEXTURES ===
file(GLOB TEXTURES "${CMAKE_SOURCE_DIR}/models/textures/*.*")
foreach(TEX ${TEXTURES})
    get_filename_component(TEXTURE ${TEX} NAME)
    add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${TEX} "${CMAKE_BINARY_DIR}/models/textures/${TEXTURE}"
            COMMENT "Copying ${TEXTURE}"
    )
endforeach()

# === COPY MODELS ===
file(GLOB MODELS "${CMAKE_SOURCE_DIR}/models/*.*")
foreach(MODEL ${MODELS})
    get_filename_component(MODEL_NAME ${MODEL} NAME)
    add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${MODEL} "${CMAKE_BINARY_DIR}/models/${MODEL_NAME}"
            COMMENT "Copying ${MODEL_NAME}"
    )
endforeach()

# === INCLUDE PATHS ===
target_include_directories(${PROJECT_NAME}
        PRIVATE ${CMAKE_SOURCE_DIR}/src
        SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/libs
        INTERFACE ${CMAKE_SOURCE_DIR}/libs
)

# === DEPENDENCIES ===
include(FetchContent)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG master
)
FetchContent_MakeAvailable(glfw)
target_link_libraries(${PROJECT_NAME} glfw)

FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG master
)
FetchContent_MakeAvailable(assimp)
target_link_libraries(${PROJECT_NAME} assimp)

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG master
)
FetchContent_MakeAvailable(glm)
target_link_libraries(${PROJECT_NAME} glm::glm-header-only)

FetchContent_Declare(
        VMA
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG master
)
FetchContent_MakeAvailable(VMA)
target_include_directories(${PROJECT_NAME} PRIVATE ${vma_SOURCE_DIR}/include)

FetchContent_Declare(
        vulkan_hpp
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Hpp.git
        GIT_TAG v1.3.290
)
FetchContent_MakeAvailable(vulkan_hpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${vulkan_hpp_SOURCE_DIR})

# === VULKAN LINK ===
find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
