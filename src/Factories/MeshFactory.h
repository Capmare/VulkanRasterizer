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
#include "vma/vk_mem_alloc.h"
#include "glm/glm.hpp"
#include "vulkan/vulkan_raii.hpp"


struct MVP {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;

    glm::vec3 cameraPos;
    float _pad0 = 0.0f;          // padding to align next item

    glm::vec2 screenSize;
    glm::vec2 _pad1 = glm::vec2(0.0f); // padding to 16 bytes
};

struct Material {
    int diffuseIdx = -1;
    int normalIdx = -1;
    int metallicIdx = -1;
    int roughnessIdx = -1;
    int aoIdx = -1;
    int emissiveIdx = -1;
};


struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
    }

    static std::array<vk::VertexInputAttributeDescription, 6> getAttributeDescriptions() {
        return {
            vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos) },
            vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) },
            vk::VertexInputAttributeDescription{ 2, 0, vk::Format::eR32G32Sfloat,    offsetof(Vertex, texCoord) },
            vk::VertexInputAttributeDescription{ 3, 0, vk::Format::eR32G32B32Sfloat,    offsetof(Vertex, normal) },
            vk::VertexInputAttributeDescription{ 4, 0, vk::Format::eR32G32B32Sfloat,    offsetof(Vertex, tangent) },
            vk::VertexInputAttributeDescription{ 5, 0, vk::Format::eR32G32B32Sfloat,    offsetof(Vertex, bitangent) }
        };
    }

};

struct Mesh
{
    BufferInfo m_VertexBufferInfo;
    VkDeviceSize m_VertexOffset{};


    BufferInfo m_IndexBufferInfo;
    VkDeviceSize m_IndexOffset;
    uint32_t m_IndexCount;

    Material m_Material;

};

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
