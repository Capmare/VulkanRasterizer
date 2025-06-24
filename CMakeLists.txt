cmake_minimum_required(VERSION 3.20)

project(VulkanRasterizer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")

if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /Zi /Ob0 /Od")
    set(CMAKE_LINKER_FLAGS_DEBUG "/DEBUG")
endif()

if(NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR "VULKAN_SDK environment variable is not set. Please make sure the Vulkan SDK is installed and initialized.")
endif()

set(GLSLC_EXECUTABLE "$ENV{VULKAN_SDK}/Bin/glslc.exe")

# Shader compile settings
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")

# Collect sources
file(GLOB_RECURSE APP_SOURCES "src/*.cpp" "src/Factories/*.cpp")

# Add executable
add_executable(${PROJECT_NAME} ${APP_SOURCES})

# Create shader output directory
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

# Shader compilation
add_custom_command(
    OUTPUT "${SHADER_OUTPUT_DIR}/vert.spv"
    COMMAND ${GLSLC_EXECUTABLE} ${SHADER_DIR}/shader.vert -o "${SHADER_OUTPUT_DIR}/vert.spv"
    DEPENDS ${SHADER_DIR}/shader.vert
    COMMENT "Compiling vertex shader"
)

add_custom_command(
    OUTPUT "${SHADER_OUTPUT_DIR}/frag.spv"
    COMMAND ${GLSLC_EXECUTABLE} ${SHADER_DIR}/shader.frag -o "${SHADER_OUTPUT_DIR}/frag.spv"
    DEPENDS ${SHADER_DIR}/shader.frag
    COMMENT "Compiling fragment shader"
)

add_custom_target(compileShaders DEPENDS "${SHADER_OUTPUT_DIR}/vert.spv" "${SHADER_OUTPUT_DIR}/frag.spv")
add_dependencies(${PROJECT_NAME} compileShaders)

# Copy shaders to build folder
file(GLOB SHADER_FILES "${SHADER_OUTPUT_DIR}/*.spv")
foreach(SHADER ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${SHADER} "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}"
        DEPENDS ${SHADER}
        COMMENT "Copying ${SHADER_NAME}"
    )
endforeach()

# Copy textures
file(GLOB TEXTURES "${CMAKE_SOURCE_DIR}/models/textures/*.*")
foreach(TEX ${TEXTURES})
    get_filename_component(TEXTURE ${TEX} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${TEX} "${CMAKE_BINARY_DIR}/models/textures/${TEXTURE}"
        COMMENT "Copying ${TEXTURE}"
    )
endforeach()

# Copy models
file(GLOB MODELS "${CMAKE_SOURCE_DIR}/models/*.*")
foreach(MODEL ${MODELS})
    get_filename_component(MODEL_NAME ${MODEL} NAME)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${MODEL} "${CMAKE_BINARY_DIR}/models/${MODEL_NAME}"
        COMMENT "Copying ${MODEL_NAME}"
    )
endforeach()

# Include paths
target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_SOURCE_DIR}/src
    SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/libs
    INTERFACE ${CMAKE_SOURCE_DIR}/libs 
)

# Debug symbol format
if (POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Fetch dependencies
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG master
)
FetchContent_MakeAvailable(glfw)
target_link_libraries(${PROJECT_NAME} glfw)

# Assimp
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG master
)
FetchContent_MakeAvailable(assimp)
target_link_libraries(${PROJECT_NAME} assimp)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG master
)
FetchContent_MakeAvailable(glm)
target_link_libraries(${PROJECT_NAME} glm::glm-header-only)

# Vulkan Memory Allocator (VMA)
FetchContent_Declare(
    VMA
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG master
)
FetchContent_MakeAvailable(VMA)
target_include_directories(${PROJECT_NAME} PRIVATE ${vma_SOURCE_DIR}/include)

# Vulkan-Hpp
FetchContent_Declare(
    vulkan_hpp
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Hpp.git
    GIT_TAG v1.3.290

)

FetchContent_MakeAvailable(vulkan_hpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${vulkan_hpp_SOURCE_DIR})

# Link Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} Vulkan::Vulkan)
