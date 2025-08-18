cmake_minimum_required(VERSION 3.24)
project(VulkanRasterizer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /Zi /Ob0 /Od")
    set(CMAKE_LINKER_FLAGS_DEBUG "/DEBUG")
endif()

if (NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR "VULKAN_SDK environment variable is not set. Please install and source the Vulkan SDK.")
endif()

set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/compiled_shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

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

add_custom_target(compileShaders ALL
        DEPENDS ${COMPILED_SHADERS}
        COMMENT "Compiling shaders if changed"
)

file(GLOB_RECURSE APP_SOURCES "src/*.cpp" "src/Factories/*.cpp")
add_executable(${PROJECT_NAME} ${APP_SOURCES})

add_dependencies(${PROJECT_NAME} compileShaders)

add_custom_target(copyCompiledShaders
        DEPENDS compileShaders
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning old shaders..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_OUTPUT_DIR} $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
        COMMENT "Copying compiled shaders to output folder"
)
add_dependencies(${PROJECT_NAME} copyCompiledShaders)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/models $<TARGET_FILE_DIR:${PROJECT_NAME}>/models
)

target_include_directories(${PROJECT_NAME}
        PRIVATE ${CMAKE_SOURCE_DIR}/src
        SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/libs
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
        NOMINMAX
)

include(FetchContent)

# GLFW
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG master
)
FetchContent_MakeAvailable(glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

# Assimp
FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG master
)
FetchContent_MakeAvailable(assimp)
target_link_libraries(${PROJECT_NAME} PRIVATE assimp)

# GLM (header-only)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG master
)
FetchContent_MakeAvailable(glm)
if (TARGET glm::glm-header-only)
    target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm-header-only)
elseif (TARGET glm::glm)
    target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
endif()

FetchContent_Declare(
        VMA
        URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.3.0.tar.gz
)
FetchContent_MakeAvailable(VMA)

if (TARGET GPUOpen::VulkanMemoryAllocator)
    target_link_libraries(${PROJECT_NAME} PRIVATE GPUOpen::VulkanMemoryAllocator)
else()
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${vma_SOURCE_DIR}/include)
endif()

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)


function(_system_includes_for this_target dep_target)
    if (TARGET ${dep_target})
        get_target_property(_incs ${dep_target} INTERFACE_INCLUDE_DIRECTORIES)
        if (_incs)
            target_include_directories(${this_target} SYSTEM PRIVATE ${_incs})
        endif()
    endif()
endfunction()

_system_includes_for(${PROJECT_NAME} glfw)
_system_includes_for(${PROJECT_NAME} assimp)
_system_includes_for(${PROJECT_NAME} glm::glm-header-only)
_system_includes_for(${PROJECT_NAME} glm::glm)
_system_includes_for(${PROJECT_NAME} GPUOpen::VulkanMemoryAllocator)
_system_includes_for(${PROJECT_NAME} Vulkan::Vulkan)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
            /external:W0
            /external:anglebrackets
    )
endif()

add_custom_command(POST_BUILD TARGET ${PROJECT_NAME}
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/circus_arena_4k.hdr $<TARGET_FILE_DIR:${PROJECT_NAME}>)