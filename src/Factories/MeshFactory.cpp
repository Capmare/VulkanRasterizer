#include "MeshFactory.h"

#include <filesystem>

#include "Buffer.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "vulkan/vulkan_raii.hpp"
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "ResourceTracker.h"
#include "ImageFactory.h"


std::vector<Mesh> MeshFactory::LoadModelFromGLTF(
    const std::string &path,
    VmaAllocator &Allocator,
    std::deque<std::function<void(VmaAllocator)>> &DeletionQueue,
    const vk::CommandBuffer &CommandBuffer,
    const vk::raii::Queue& GraphicsQueue,
    vk::raii::Device &device,
    const vk::raii::CommandPool& CmdPool,
    std::vector<ImageResource>& textures, std::vector<vk::ImageView>& TextureImageViews, ResourceTracker* AllocTracker
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
    ProcessNode = [&](aiNode* node, const aiScene* currentScene) {
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            aiMesh* mesh = currentScene->mMeshes[node->mMeshes[i]];
            aiMaterial* material = currentScene->mMaterials[mesh->mMaterialIndex];

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            aiMatrix4x4 transform = currentScene->mRootNode->mTransformation;
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
                        auto texture = ImageFactory::LoadTexture(
                                m_MeshBuffer.get(), fullPath, device, Allocator, CmdPool,
                                GraphicsQueue, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, AllocTracker
                                );

                        textures.emplace_back(texture);

                        auto ImageView = ImageFactory::CreateImageView(
                            device, texture.image, vk::Format::eR8G8B8A8Srgb,
                            vk::ImageAspectFlagBits::eColor, AllocTracker,
                            "textureImageView: " + fullPath
                            );

                        TextureImageViews.emplace_back(ImageView);

                        textureCache[fullPath] = static_cast<uint32_t>(textures.size() - 1);

                        // Schedule deletion of allocation
                        VmaAllocation allocation = textures.back().allocation;
                        DeletionQueue.emplace_back([Allocator, allocation,AllocTracker,textures,&device, TextureImageViews](VmaAllocator alloc) {
                            AllocTracker->UntrackImageView(TextureImageViews.back());
                            vkDestroyImageView(*device,TextureImageViews.back(),nullptr);
                            vkDestroyImage(*device,textures.back().image,nullptr);

                            AllocTracker->UntrackAllocation(allocation);
                            vmaFreeMemory(alloc, allocation);
                        });
                    }
                }
            }

            Mesh meshObj = Build_Mesh(
                Allocator, DeletionQueue, CommandBuffer, GraphicsQueue,
                vertices, indices, AllocTracker,
                "Mesh"
            );

            meshes.push_back(meshObj);
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            ProcessNode(node->mChildren[i], currentScene);
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
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t> indices,
    ResourceTracker* AllocationTracker,
    const std::string& AllocName
) {
    Mesh mesh{};

    mesh.m_IndexCount = static_cast<uint32_t>(indices.size());

    // Create staging and GPU buffers for vertices
    BufferInfo VertexStagingBuffer = m_MeshBuffer->CreateMapped(Allocator,vertices.size() * sizeof(Vertex),
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
        AllocationTracker,"VertexBuffer");

    Buffer::UploadData(VertexStagingBuffer, vertices.data(), vertices.size() * sizeof(Vertex));

    mesh.m_VertexBufferInfo = m_MeshBuffer->CreateMapped(Allocator,
                   vertices.size() * sizeof(Vertex),
                   vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                   VMA_MEMORY_USAGE_AUTO,0,AllocationTracker,AllocName);


    Buffer::Copy(VertexStagingBuffer.m_Buffer, VertexStagingBuffer.m_AllocInfo,
                 mesh.m_VertexBufferInfo.m_Buffer, mesh.m_VertexBufferInfo.m_AllocInfo,
                 GraphicsQueue, CommandBuffer);

    Buffer::Destroy(Allocator, VertexStagingBuffer.m_Buffer, VertexStagingBuffer.m_Allocation, AllocationTracker);

    // Create staging and GPU buffers for indices
    BufferInfo IndexStagingBuffer = m_MeshBuffer->CreateMapped(Allocator,indices.size() * sizeof(indices[0]),
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
        AllocationTracker,"IndexStagingBuffer");

    Buffer::UploadData(IndexStagingBuffer, indices.data(), indices.size() * sizeof(indices[0]));

    mesh.m_IndexBufferInfo = m_MeshBuffer->CreateMapped(
        Allocator,indices.size() * sizeof(indices[0]),
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO,
        0, AllocationTracker,"IndexBuffer");


    Buffer::Copy(IndexStagingBuffer.m_Buffer, IndexStagingBuffer.m_AllocInfo,
                 mesh.m_IndexBufferInfo.m_Buffer, mesh.m_IndexBufferInfo.m_AllocInfo,
                 GraphicsQueue, CommandBuffer);

    Buffer::Destroy(Allocator, IndexStagingBuffer.m_Buffer, IndexStagingBuffer.m_Allocation, AllocationTracker);


    // Setup deletion queue
    auto vb = mesh.m_VertexBufferInfo.m_Buffer;
    auto va = mesh.m_VertexBufferInfo.m_Allocation;
    auto ib = mesh.m_IndexBufferInfo.m_Buffer;
    auto ia = mesh.m_IndexBufferInfo.m_Allocation;
    // auto ub = mesh.m_UniformBuffer;
    // auto ua = mesh.m_UniformAllocation;

    DeletionQueue.emplace_back([vb, va, ib, ia,AllocationTracker](VmaAllocator allocator) {
        Buffer::Destroy(allocator, vb, va, AllocationTracker);
        Buffer::Destroy(allocator, ib, ia, AllocationTracker);
    });

    return mesh;
}
