#include "MeshFactory.h"
#include "Buffer.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "vulkan/vulkan_raii.hpp"

Mesh MeshFactory::Build_Triangle(
    VmaAllocator &Allocator,
    std::deque<std::function<void(VmaAllocator)>> &DeletionQueue,
    const vk::CommandBuffer &CommandBuffer,
    vk::Queue GraphicsQueue,
    vk::raii::Device &device,
    vk::DescriptorPool descriptorPool,
    vk::DescriptorSetLayout descriptorSetLayout
) {
    Mesh mesh;

    // === Vertex & Index Data ===
    // 8 vertices for two squares (front and back faces)
    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}},

        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
    };

    const std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4
    };
    mesh.m_IndexCount = static_cast<uint32_t>(indices.size());

    // === Vertex Staging Buffer ===
    VkBuffer vertexStagingBuffer;
    VmaAllocation vertexStagingAlloc;
    VmaAllocationInfo vertexStagingInfo{};
    Buffer::Create(Allocator,
                   vertices.size() * sizeof(Vertex), // Correct size here!
                   vk::BufferUsageFlagBits::eTransferSrc,
                   VMA_MEMORY_USAGE_AUTO,
                   vertexStagingBuffer,
                   vertexStagingAlloc,
                   vertexStagingInfo,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT);
    Buffer::UploadData(Allocator, vertexStagingAlloc, vertices.data(), vertices.size() * sizeof(Vertex));  // Correct size here!

    // === Vertex GPU Buffer ===
    Buffer::Create(Allocator,
                   vertices.size() * sizeof(Vertex),
                   vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                   VMA_MEMORY_USAGE_AUTO,
                   mesh.m_VertexBuffer,
                   mesh.m_VertexAllocation,
                   mesh.m_VertexAllocInfo);

    Buffer::Copy(vertexStagingBuffer, vertexStagingInfo,
                 mesh.m_VertexBuffer, mesh.m_VertexAllocInfo,
                 GraphicsQueue, CommandBuffer);

    Buffer::Destroy(Allocator, vertexStagingBuffer, vertexStagingAlloc);

    // === Index Staging Buffer ===
    VkBuffer indexStagingBuffer;
    VmaAllocation indexStagingAlloc;
    VmaAllocationInfo indexStagingInfo{};
    Buffer::Create(Allocator,
                   indices.size() * sizeof(uint16_t),
                   vk::BufferUsageFlagBits::eTransferSrc,
                   VMA_MEMORY_USAGE_AUTO,
                   indexStagingBuffer,
                   indexStagingAlloc,
                   indexStagingInfo,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT);
    Buffer::UploadData(Allocator, indexStagingAlloc, indices.data(), indices.size() * sizeof(uint16_t));

    // === Index GPU Buffer ===
    Buffer::Create(Allocator,
                   indices.size() * sizeof(uint16_t),
                   vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                   VMA_MEMORY_USAGE_AUTO,
                   mesh.m_IndexBuffer,
                   mesh.m_IndexAllocation,
                   mesh.m_IndexAllocInfo);

    Buffer::Copy(indexStagingBuffer, indexStagingInfo,
                 mesh.m_IndexBuffer, mesh.m_IndexAllocInfo,
                 GraphicsQueue, CommandBuffer);

    Buffer::Destroy(Allocator, indexStagingBuffer, indexStagingAlloc);

    // === Uniform Buffer (MVP) ===
    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
                           glm::vec3(0.0f),
                           glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f),
                                1.0f,
                                0.1f,
                                10.0f);
    ubo.proj[1][1] *= -1; // Flip Y for Vulkan

    Buffer::Create(Allocator,
                   sizeof(ubo),
                   vk::BufferUsageFlagBits::eUniformBuffer,
                   VMA_MEMORY_USAGE_CPU_TO_GPU,
                   mesh.m_UniformBuffer,
                   mesh.m_UniformAllocation,
                   mesh.m_UniformAllocInfo,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    Buffer::UploadData(Allocator, mesh.m_UniformAllocation, &ubo, sizeof(ubo));

    // === Descriptor Set ===
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    auto sets = vk::raii::DescriptorSets(device, allocInfo);
    mesh.DescriptorSet = std::make_unique<vk::raii::DescriptorSet>(std::move(sets.front()));

    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = mesh.m_UniformBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(UniformBufferObject);

    vk::WriteDescriptorSet descriptorWrite{};
    descriptorWrite.dstSet = **mesh.DescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    device.updateDescriptorSets(descriptorWrite, nullptr);

    // === Deletion Queue ===
    auto vb = mesh.m_VertexBuffer;
    auto va = mesh.m_VertexAllocation;
    auto ib = mesh.m_IndexBuffer;
    auto ia = mesh.m_IndexAllocation;
    auto ub = mesh.m_UniformBuffer;
    auto ua = mesh.m_UniformAllocation;

    DeletionQueue.emplace_back([vb, va, ib, ia, ub, ua](VmaAllocator allocator) {
        Buffer::Destroy(allocator, vb, va);
        Buffer::Destroy(allocator, ib, ia);
        Buffer::Destroy(allocator, ub, ua);
    });

    return mesh;
}
