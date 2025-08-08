//
// Created by capma on 8/8/2025.
//

#ifndef MESH_H
#define MESH_H

#include <vulkan/vulkan.hpp>
#include "Buffer.h"

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

    std::vector<glm::vec3> m_Positions;
};


#endif //MESH_H
