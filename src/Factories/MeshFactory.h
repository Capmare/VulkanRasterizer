//
// Created by capma on 7/3/2025.
//

#ifndef MESHFACTORY_H
#define MESHFACTORY_H
#include <deque>

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include <vulkan/vulkan.hpp>

#include "ImageFactory.h"
#include "assimp/scene.h"
#include "vk_mem_alloc.h"
#include "glm/glm.hpp"
#include "vulkan/vulkan_raii.hpp"
#include "Structs/Mesh.h"

class MeshFactory {

public:
    MeshFactory() = default;
    virtual ~MeshFactory() = default;

    MeshFactory(const MeshFactory&) = delete;
    MeshFactory(MeshFactory&&) noexcept = delete;
    MeshFactory& operator=(const MeshFactory&) = delete;
    MeshFactory& operator=(MeshFactory&&) noexcept = delete;


    std::vector<Mesh> LoadModelFromGLTF(const std::string &path, VmaAllocator &Allocator,
                                        std::deque<std::function<void(VmaAllocator)>> &DeletionQueue,
                                        const vk::CommandBuffer &CommandBuffer, const vk::raii::Queue &GraphicsQueue,
                                        vk::raii::Device &device,
                                        const vk::raii::CommandPool &CmdPool, std::vector<ImageResource> &textures, std::vector<vk::ImageView> &
                                        TextureImageViews, class ResourceTracker *AllocTracker);

    Mesh Build_Mesh(VmaAllocator &Allocator, std::deque<std::function<void(VmaAllocator)>> &DeletionQueue,
                    const vk::CommandBuffer &CommandBuffer, vk::Queue GraphicsQueue,
                    const std::vector<Vertex> &vertices, std::vector<uint32_t> indices, ResourceTracker *AllocationTracker, const std::
                    string &AllocName);

    int LoadTextureGeneric(const aiScene *scene, const std::string &texPathStr, const std::filesystem::path &baseDir,
                           std::unordered_map<std::string, uint32_t> &textureCache,
                           std::vector<ImageResource> &textures,
                           std::vector<vk::ImageView> &textureImageViews, VmaAllocator &allocator,
                           std::deque<std::function<void(VmaAllocator)>> &deletionQueue, vk::raii::Device &device,
                           const vk::raii::CommandPool &cmdPool, const vk::raii::Queue &graphicsQueue,
                           ResourceTracker *allocTracker, vk::Format format);

private:
    std::unique_ptr<Buffer> m_MeshBuffer = std::make_unique<Buffer>();

};



#endif //MESHFACTORY_H
