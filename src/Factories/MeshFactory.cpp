//
// Created by capma on 7/3/2025.
//

#include "MeshFactory.h"

#include "Buffer.h"
#include "vulkan/vulkan_raii.hpp"

Mesh MeshFactory::Build_Triangle(VmaAllocator&Allocator, std::deque<std::function<void(VmaAllocator)>> &DeletionQueue, const vk::CommandBuffer &CommandBuffer, vk::Queue GraphicsQueue) {

    Mesh Mesh;

    VkBuffer VertexBuffer = VK_NULL_HANDLE;
    VkBuffer StagingBuffer = VK_NULL_HANDLE;

    VmaAllocation VertexAllocation = VK_NULL_HANDLE;
    VmaAllocationInfo VertexAllocationInfo{};

    VmaAllocation StagingAllocation = VK_NULL_HANDLE;
    VmaAllocationInfo StagingAllocationInfo{};



    vk::BufferCreateInfo VertexBufferCreateInfo{};
    VertexBufferCreateInfo.flags = vk::BufferCreateFlags();
    VertexBufferCreateInfo.size = 3 * sizeof(Vertex);
    VertexBufferCreateInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

    VkBufferCreateInfo BufferInfoHandle = VertexBufferCreateInfo;
    VmaAllocationCreateInfo AllocationCreateInfo{};
    AllocationCreateInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;


    vmaCreateBuffer(Allocator, &BufferInfoHandle, &AllocationCreateInfo,&StagingBuffer,&StagingAllocation, &StagingAllocationInfo);


    Vertex vertices[3] = {
        {{-0.75f,  0.75f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.75f,  0.75f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.0f,  -0.75f}, {0.0f, 0.0f, 1.0f}}
    };

    void* dst;
    vmaMapMemory(Allocator,StagingAllocation,&dst);
    memcpy(dst,vertices,3*sizeof(Vertex));
    vmaUnmapMemory(Allocator,StagingAllocation);

    VertexBufferCreateInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
    BufferInfoHandle = VertexBufferCreateInfo;
    AllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;

    vmaCreateBuffer(Allocator, &BufferInfoHandle, &AllocationCreateInfo,&VertexBuffer,&VertexAllocation, &VertexAllocationInfo);

    Buffer::Copy(StagingBuffer,StagingAllocationInfo,VertexBuffer,VertexAllocationInfo,GraphicsQueue,CommandBuffer);

    Mesh.m_Buffer = VertexBuffer;
    Mesh.m_Allocation = VertexAllocation;
    Mesh.m_Offset = VertexAllocationInfo.offset;


    vmaSetAllocationName(Allocator,VertexAllocation,"VertexBuffer");
    vmaSetAllocationName(Allocator,StagingAllocation,"StagingBuffer");

    vmaDestroyBuffer(Allocator,StagingBuffer,StagingAllocation);

    DeletionQueue.push_back([Mesh](VmaAllocator Allocator) {
       vmaDestroyBuffer(Allocator,Mesh.m_Buffer,Mesh.m_Allocation);
    });

    return Mesh;
}
