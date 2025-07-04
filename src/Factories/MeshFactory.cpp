//
// Created by capma on 7/3/2025.
//

#include "MeshFactory.h"

Mesh MeshFactory::Build_Triangle(VmaAllocator&Allocator, std::deque<std::function<void(VmaAllocator)>> &DeletionQueue) {

    Mesh Mesh;

    vk::BufferCreateInfo VertexBufferCreateInfo{};
    VertexBufferCreateInfo.flags = vk::BufferCreateFlags();
    VertexBufferCreateInfo.size = 3 * sizeof(Vertex);
    VertexBufferCreateInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;

    VkBufferCreateInfo BufferInfoHandle = VertexBufferCreateInfo;
    VmaAllocationCreateInfo AllocationCreateInfo{};
    AllocationCreateInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkBuffer VertexBuffer = VK_NULL_HANDLE;
    VmaAllocationInfo VertexAllocationInfo{};
    vmaCreateBuffer(Allocator, &BufferInfoHandle, &AllocationCreateInfo,&VertexBuffer,&Mesh.m_Allocation, &VertexAllocationInfo);

    Mesh.m_Buffer = VertexBuffer;
    Mesh.m_Offset = VertexAllocationInfo.offset;

    Vertex vertices[3] = {
        {{-0.75f,  0.75f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.75f,  0.75f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.0f,  -0.75f}, {0.0f, 0.0f, 1.0f}}
    };

    void* dst;
    vmaMapMemory(Allocator,Mesh.m_Allocation,&dst);
    memcpy(dst,vertices,3*sizeof(Vertex));
    vmaUnmapMemory(Allocator,Mesh.m_Allocation);

    DeletionQueue.push_back([Mesh](VmaAllocator Allocator) {
       vmaDestroyBuffer(Allocator,Mesh.m_Buffer,Mesh.m_Allocation);
    });

    return Mesh;
}
