#include "MeshFactory.h"

#include <filesystem>

#include "Buffer.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "vulkan/vulkan_raii.hpp"
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "ImageFactory.h"


std::vector<Mesh> MeshFactory::LoadModelFromGLTF(
    const std::string &path,
    VmaAllocator &Allocator,
    std::deque<std::function<void(VmaAllocator)>> &DeletionQueue,
    const vk::CommandBuffer &CommandBuffer,
    const vk::raii::Queue& GraphicsQueue,
    vk::raii::Device &device,
    vk::DescriptorPool descriptorPool,
    vk::DescriptorSetLayout descriptorSetLayout,
    vk::Sampler sampler,
    const vk::raii::CommandPool& CmdPool,
    std::vector<ImageResource>& textures
    )
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        throw std::runtime_error("Failed to load model: " + std::filesystem::absolute(path).string());

    std::filesystem::path baseDir = std::filesystem::path(path).parent_path();

    std::unordered_map<std::string, uint32_t> textureCache;

    std::vector<Mesh> meshes;

    std::function<void(aiNode*, const aiScene*)> ProcessNode;
    ProcessNode = [&](aiNode* node, const aiScene* scene) {
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            aiMatrix4x4 transform = scene->mRootNode->mTransformation;
            for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
                Vertex vert{};
                aiVector3D const aiPosition = transform * mesh->mVertices[v];
                vert.pos = glm::vec3(aiPosition.x, aiPosition.y, aiPosition.z);

                //vert.normal = mesh->HasNormals() ? glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z) : glm::vec3(0.0f);
                if (mesh->HasTextureCoords(0))
                    vert.texCoord = glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
                else
                    vert.texCoord = glm::vec2(0.0f);
                vertices.push_back(vert);
            }

            for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                const aiFace& face = mesh->mFaces[f];
                for (unsigned int j = 0; j < face.mNumIndices; ++j)
                    indices.push_back(face.mIndices[j]);
            }


            int textureIdx{};

            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString texPath;
                if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
                    std::string fullPath = (baseDir / texPath.C_Str()).string();

                    if (textureCache.contains(fullPath)) {
                        textureIdx = textureCache[fullPath];
                    } else {
                        textures.emplace_back(ImageFactory::LoadTexture(fullPath, device, Allocator, CmdPool, GraphicsQueue, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor));

                        textureCache[fullPath] = textures.size() - 1;

                        // Schedule deletion of allocation
                        VmaAllocation allocation = textures.back().allocation;
                        DeletionQueue.push_back([Allocator, allocation](VmaAllocator alloc) {
                            vmaFreeMemory(alloc, allocation);
                        });
                    }
                }
            }

            Mesh meshObj = Build_Mesh(
                Allocator, DeletionQueue, CommandBuffer, GraphicsQueue,
                device, descriptorPool, descriptorSetLayout,
                textureIdx, sampler, vertices, indices
            );

            meshes.push_back(std::move(meshObj));
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            ProcessNode(node->mChildren[i], scene);
    };

    ProcessNode(scene->mRootNode, scene);

    return meshes;
}



// Add textureImageView and sampler as parameters to bind texture
Mesh MeshFactory::Build_Mesh(
    VmaAllocator &Allocator,
    std::deque<std::function<void(VmaAllocator)>> &DeletionQueue,
    const vk::CommandBuffer &CommandBuffer,
    vk::Queue GraphicsQueue,
    vk::raii::Device &device,
    vk::DescriptorPool descriptorPool,
    vk::DescriptorSetLayout descriptorSetLayout,
    uint32_t ImageIdx,
    vk::Sampler sampler,
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t> indices
) {
    Mesh mesh;

    mesh.m_IndexCount = static_cast<uint32_t>(indices.size());

    // Create staging and GPU buffers for vertices
    VkBuffer vertexStagingBuffer;
    VmaAllocation vertexStagingAlloc;
    VmaAllocationInfo vertexStagingInfo{};
    Buffer::Create(Allocator,
                   vertices.size() * sizeof(Vertex),
                   vk::BufferUsageFlagBits::eTransferSrc,
                   VMA_MEMORY_USAGE_AUTO,
                   vertexStagingBuffer,
                   vertexStagingAlloc,
                   vertexStagingInfo,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT);
    Buffer::UploadData(Allocator, vertexStagingAlloc, vertices.data(), vertices.size() * sizeof(Vertex));

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

    // Create staging and GPU buffers for indices
    VkBuffer indexStagingBuffer;
    VmaAllocation indexStagingAlloc;
    VmaAllocationInfo indexStagingInfo{};
    Buffer::Create(Allocator,
                   indices.size() * sizeof(indices[0]),
                   vk::BufferUsageFlagBits::eTransferSrc,
                   VMA_MEMORY_USAGE_AUTO,
                   indexStagingBuffer,
                   indexStagingAlloc,
                   indexStagingInfo,
                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT);
    Buffer::UploadData(Allocator, indexStagingAlloc, indices.data(), indices.size() * sizeof(indices[0]));

    Buffer::Create(Allocator,
                   indices.size() * sizeof(indices[0]),
                   vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                   VMA_MEMORY_USAGE_AUTO,
                   mesh.m_IndexBuffer,
                   mesh.m_IndexAllocation,
                   mesh.m_IndexAllocInfo);

    Buffer::Copy(indexStagingBuffer, indexStagingInfo,
                 mesh.m_IndexBuffer, mesh.m_IndexAllocInfo,
                 GraphicsQueue, CommandBuffer);

    Buffer::Destroy(Allocator, indexStagingBuffer, indexStagingAlloc);


    // Setup deletion queue
    auto vb = mesh.m_VertexBuffer;
    auto va = mesh.m_VertexAllocation;
    auto ib = mesh.m_IndexBuffer;
    auto ia = mesh.m_IndexAllocation;
    // auto ub = mesh.m_UniformBuffer;
    // auto ua = mesh.m_UniformAllocation;

    DeletionQueue.emplace_back([vb, va, ib, ia](VmaAllocator allocator) {
        Buffer::Destroy(allocator, vb, va);
        Buffer::Destroy(allocator, ib, ia);
    });

    return mesh;
}
