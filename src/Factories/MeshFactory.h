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
#include "vma/vk_mem_alloc.h"
#include "glm/glm.hpp"
#include "vulkan/vulkan_raii.hpp"


struct MVP {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};


struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return { 0, sizeof(Vertex), vk::VertexInputRate::eVertex };
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        return {
            vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos) },
            vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color) },
            vk::VertexInputAttributeDescription{ 2, 0, vk::Format::eR32G32Sfloat,    offsetof(Vertex, texCoord) }
        };
    }
};

struct Mesh
{
    VkBuffer m_VertexBuffer{};
    VmaAllocation m_VertexAllocation{};
    VmaAllocationInfo m_VertexAllocInfo{};
    VkDeviceSize m_VertexOffset{};

    VkBuffer m_IndexBuffer;
    VmaAllocation m_IndexAllocation;
    VmaAllocationInfo m_IndexAllocInfo{};
    VkDeviceSize m_IndexOffset;
    uint32_t m_IndexCount;

    uint32_t m_TextureIdx{};

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
                                        vk::raii::Device &device, vk::DescriptorPool descriptorPool,
                                        vk::DescriptorSetLayout descriptorSetLayout, vk::Sampler sampler, const vk::raii::CommandPool &CmdPool, std::vector<
                                            ImageResource> &textures, class ResourceTracker* AllocTracker);

    Mesh Build_Mesh(VmaAllocator &Allocator, std::deque<std::function<void(VmaAllocator)>> &DeletionQueue,
                    const vk::CommandBuffer &CommandBuffer, vk::Queue GraphicsQueue, vk::raii::Device &device,
                    vk::DescriptorPool descriptorPool, vk::DescriptorSetLayout descriptorSetLayout, uint32_t ImageIdx, vk::Sampler sampler, const
                    std::vector<Vertex> &vertices, std::vector<uint32_t> indices, ResourceTracker *AllocationTracker, const std::string &
                    AllocName);






};



#endif //MESHFACTORY_H
