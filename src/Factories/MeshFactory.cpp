//
// Created by capma on 7/3/2025.
//

#include "MeshFactory.h"

#include "Buffer.h"
#include "vulkan/vulkan_raii.hpp"

Mesh MeshFactory::Build_Triangle(VmaAllocator& Allocator, std::deque<std::function<void(VmaAllocator)>> &DeletionQueue, const vk::CommandBuffer &CommandBuffer, vk::Queue GraphicsQueue) {
    Mesh Mesh;

    Vertex vertices[3] = {
        {{-0.75f,  0.75f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.75f,  0.75f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.0f,  -0.75f}, {0.0f, 0.0f, 1.0f}}
    };

    uint32_t indices[3] = { 0, 1, 2 };
    Mesh.m_IndexCount = 3;

    VkBuffer vertexStagingBuffer;
    VmaAllocation vertexStagingAllocation;
    VmaAllocationInfo vertexStagingInfo{};

    vk::BufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.size = sizeof(vertices);
    stagingBufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

    VkBufferCreateInfo stagingVk = stagingBufferInfo;
    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    vmaCreateBuffer(Allocator, &stagingVk, &stagingAllocInfo, &vertexStagingBuffer, &vertexStagingAllocation, &vertexStagingInfo);
    void* dst;
    vmaMapMemory(Allocator, vertexStagingAllocation, &dst);
    memcpy(dst, vertices, sizeof(vertices));
    vmaUnmapMemory(Allocator, vertexStagingAllocation);

    vk::BufferCreateInfo vertexBufferInfo{};
    vertexBufferInfo.size = sizeof(vertices);
    vertexBufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;

    VkBufferCreateInfo vertexVk = vertexBufferInfo;
    VmaAllocationCreateInfo vertexAllocInfo{};
    vertexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vertexAllocInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

    VkBuffer vertexBuffer;
    VmaAllocation vertexAllocation;
    VmaAllocationInfo vertexAllocInfoOut{};
    vmaCreateBuffer(Allocator, &vertexVk, &vertexAllocInfo, &vertexBuffer, &vertexAllocation, &vertexAllocInfoOut);

    Buffer::Copy(vertexStagingBuffer, vertexStagingInfo, vertexBuffer, vertexAllocInfoOut, GraphicsQueue, CommandBuffer);

    Mesh.m_Buffer = vertexBuffer;
    Mesh.m_Allocation = vertexAllocation;
    Mesh.m_Offset = vertexAllocInfoOut.offset;

    vmaDestroyBuffer(Allocator, vertexStagingBuffer, vertexStagingAllocation);

    VkBuffer indexStagingBuffer;
    VmaAllocation indexStagingAllocation;
    VmaAllocationInfo indexStagingInfo{};

    vk::BufferCreateInfo indexStagingInfoVk{};
    indexStagingInfoVk.size = sizeof(indices);
    indexStagingInfoVk.usage = vk::BufferUsageFlagBits::eTransferSrc;

    VkBufferCreateInfo indexVk = indexStagingInfoVk;
    VmaAllocationCreateInfo indexAllocInfo{};
    indexAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    indexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    vmaCreateBuffer(Allocator, &indexVk, &indexAllocInfo, &indexStagingBuffer, &indexStagingAllocation, &indexStagingInfo);
    vmaMapMemory(Allocator, indexStagingAllocation, &dst);
    memcpy(dst, indices, sizeof(indices));
    vmaUnmapMemory(Allocator, indexStagingAllocation);

    vk::BufferCreateInfo indexBufferInfo{};
    indexBufferInfo.size = sizeof(indices);
    indexBufferInfo.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;

    VkBuffer indexBuffer;
    VmaAllocation indexAllocation;
    VmaAllocationInfo indexAllocOut{};
    vmaCreateBuffer(Allocator, reinterpret_cast<VkBufferCreateInfo *>(&indexBufferInfo), &vertexAllocInfo, &indexBuffer, &indexAllocation, &indexAllocOut);

    Buffer::Copy(indexStagingBuffer, indexStagingInfo, indexBuffer, indexAllocOut, GraphicsQueue, CommandBuffer);

    Mesh.m_IndexBuffer = indexBuffer;
    Mesh.m_IndexAllocation = indexAllocation;
    Mesh.m_IndexOffset = indexAllocOut.offset;

    vmaDestroyBuffer(Allocator, indexStagingBuffer, indexStagingAllocation);

    vmaSetAllocationName(Allocator, vertexAllocation, "VertexBuffer");
    vmaSetAllocationName(Allocator, indexAllocation, "IndexBuffer");

    DeletionQueue.push_back([mesh = Mesh](VmaAllocator allocator) {
        vmaDestroyBuffer(allocator, mesh.m_Buffer, mesh.m_Allocation);
        vmaDestroyBuffer(allocator, mesh.m_IndexBuffer, mesh.m_IndexAllocation);
    });

    return Mesh;
}
