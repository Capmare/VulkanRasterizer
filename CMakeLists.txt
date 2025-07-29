cmake_minimum_required(VERSION 3.20)
project(VulkanRasterizer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === MSVC warnings ===
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /Zi /Ob0 /Od")
    set(CMAKE_LINKER_FLAGS_DEBUG "/DEBUG")
endif()

# === Vulkan SDK ===
if(NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR "VULKAN_SDK environment variable is not set. Please install and source the Vulkan SDK.")
endif()

# === Shader Paths ===
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/compiled_shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

# === Compile shaders if source changed ===
file(GLOB SHADER_SOURCES "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag")
set(COMPILED_SHADERS "")

foreach(SHADER_FILE ${SHADER_SOURCES})
    get_filename_component(SHADER_BASENAME ${SHADER_FILE} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER_FILE} EXT)
    string(SUBSTRING ${SHADER_EXT} 1 999 SHADER_STAGE)

    set(SPIRV_FILE "${SHADER_OUTPUT_DIR}/${SHADER_BASENAME}${SHADER_STAGE}.spv")

    add_custom_command(
            OUTPUT ${SPIRV_FILE}
            COMMAND "$ENV{VULKAN_SDK}/Bin/glslc.exe" ${SHADER_FILE} -o ${SPIRV_FILE}
            DEPENDS ${SHADER_FILE}
            COMMENT "Compiling shader ${SHADER_BASENAME}.${SHADER_STAGE}"
            VERBATIM
    )

    list(APPEND COMPILED_SHADERS ${SPIRV_FILE})
endforeach()

add_custom_target(compileShaders
        DEPENDS ${COMPILED_SHADERS}
        COMMENT "Compiling shaders if changed"
)

# === Copy compiled shaders to runtime folder ===
add_custom_target(copyCompiledShaders ALL
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning old shaders..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_OUTPUT_DIR} $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
        COMMENT "Copying compiled shaders to output folder"
)

# === App Sources ===
file(GLOB_RECURSE APP_SOURCES "src/*.cpp" "src/Factories/*.cpp")
add_executable(${PROJECT_NAME} ${APP_SOURCES})

# === Build shaders and copy them before app ===
add_dependencies(${PROJECT_NAME} compileShaders copyCompiledShaders)

# === Copy models to output folder ===
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/models $<TARGET_FILE_DIR:${PROJECT_NAME}>/models
)

# === Include Directories ===
target_include_directories(${PROJECT_NAME}
        PRIVATE ${CMAKE_SOURCE_DIR}/src
        SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/libs
        INTERFACE ${CMAKE_SOURCE_DIR}/libs
)

# === Fetch and link external dependencies ===
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

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
