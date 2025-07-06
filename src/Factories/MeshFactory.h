//
// Created by capma on 7/3/2025.
//

#ifndef MESHFACTORY_H
#define MESHFACTORY_H
#include <deque>

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include <vulkan/vulkan.hpp>

#include "vma/vk_mem_alloc.h"
#include "glm/glm.hpp"
#include "vulkan/vulkan_raii.hpp"


struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};


struct Vertex {
    glm::vec3 position;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = vk::VertexInputRate::eVertex;
        return bindingDescription;
    }

    static std::vector<vk::VertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(2);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
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

    VkBuffer m_UniformBuffer;
    VmaAllocation m_UniformAllocation;
    VmaAllocationInfo m_UniformAllocInfo;

    std::unique_ptr<vk::raii::DescriptorSet> DescriptorSet;

    void UpdateUbo(std::function<void(UniformBufferObject&)> updateFunc, UniformBufferObject& ubo) {
        updateFunc(ubo);
    }
};

class MeshFactory {

public:
    MeshFactory() = default;
    virtual ~MeshFactory() = default;

    MeshFactory(const MeshFactory&) = delete;
    MeshFactory(MeshFactory&&) noexcept = delete;
    MeshFactory& operator=(const MeshFactory&) = delete;
    MeshFactory& operator=(MeshFactory&&) noexcept = delete;


    Mesh Build_Triangle(VmaAllocator &Allocator, std::deque<std::function<void(VmaAllocator)>> &DeletionQueue,
                        const vk::CommandBuffer &CommandBuffer, vk::Queue GraphicsQueue, vk::raii::Device &device,
                        vk::DescriptorPool descriptorPool, vk::DescriptorSetLayout descriptorSetLayout);

};



#endif //MESHFACTORY_H
