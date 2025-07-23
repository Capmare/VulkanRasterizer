#include "MeshFactory.h"

#include <filesystem>
#include <unordered_map>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include "glm/ext/matrix_transform.hpp"
#include "Buffer.h"
#include "ImageFactory.h"
#include "ResourceTracker.h"

std::vector<Mesh> MeshFactory::LoadModelFromGLTF(
    const std::string &path,
    VmaAllocator &allocator,
    std::deque<std::function<void(VmaAllocator)>> &deletionQueue,
    const vk::CommandBuffer &commandBuffer,
    const vk::raii::Queue& graphicsQueue,
    vk::raii::Device &device,
    const vk::raii::CommandPool& cmdPool,
    std::vector<ImageResource>& textures,
    std::vector<vk::ImageView>& textureImageViews,
    ResourceTracker* allocTracker
) {
    // Load scene with Assimp
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        throw std::runtime_error("Failed to load model: " + std::filesystem::absolute(path).string());
    }

    std::filesystem::path baseDir = std::filesystem::path(path).parent_path();
    std::unordered_map<std::string, uint32_t> textureCache;
    std::vector<Mesh> meshes;

    std::function<void(aiNode*, const aiScene*)> processNode;
    processNode = [&](aiNode* node, const aiScene* currentScene) {
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            aiMesh* mesh = currentScene->mMeshes[node->mMeshes[i]];
            aiMaterial* material = currentScene->mMaterials[mesh->mMaterialIndex];

            // Process vertices
            std::vector<Vertex> vertices;
            vertices.reserve(mesh->mNumVertices);
            aiMatrix4x4 transform = currentScene->mRootNode->mTransformation;

            for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
                Vertex vert{};
                aiVector3D aiPosition = transform * mesh->mVertices[v];
                vert.pos = glm::vec3(aiPosition.x, aiPosition.y, aiPosition.z);

                if (mesh->HasTextureCoords(0)) {
                    vert.texCoord = glm::vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
                } else {
                    vert.texCoord = glm::vec2(0.0f);
                }
                vertices.push_back(vert);
            }

            // Process indices
            std::vector<uint32_t> indices;
            for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
                const aiFace& face = mesh->mFaces[f];
                for (unsigned int j = 0; j < face.mNumIndices; ++j)
                    indices.push_back(face.mIndices[j]);
            }

            int textureIdx = -1;
            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString texPath;
                if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS) {
                    std::string fullPath = (baseDir / texPath.C_Str()).string();

                    if (textureCache.contains(fullPath)) {
                        textureIdx = textureCache[fullPath];
                    } else {
                        // Load texture and create ImageView
                        auto texture = ImageFactory::LoadTexture(
                            m_MeshBuffer.get(), fullPath, device, allocator, cmdPool,
                            graphicsQueue, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, allocTracker);

                        textures.emplace_back(texture);
                        auto imageView = ImageFactory::CreateImageView(
                            device, texture.image, vk::Format::eR8G8B8A8Srgb,
                            vk::ImageAspectFlagBits::eColor, allocTracker,
                            "textureImageView: " + fullPath);

                        textureImageViews.emplace_back(imageView);
                        textureIdx = static_cast<int>(textures.size() - 1);
                        textureCache[fullPath] = textureIdx;

                        VmaAllocation allocation = texture.allocation;
                        vk::Image imgHandle = texture.image;
                        vk::ImageView imgViewHandle = imageView;

                        deletionQueue.emplace_back([allocation, imgHandle, imgViewHandle, &device, allocTracker](VmaAllocator alloc) {
                            allocTracker->UntrackImageView(imgViewHandle);
                            vkDestroyImageView(*device, imgViewHandle, nullptr);

                            vkDestroyImage(*device, imgHandle, nullptr);
                            allocTracker->UntrackAllocation(allocation);
                            vmaFreeMemory(alloc, allocation);
                        });
                    }
                }
            }

            // Build GPU buffers for mesh
            Mesh meshObj = Build_Mesh(
                allocator, deletionQueue, commandBuffer, graphicsQueue,
                vertices, indices, allocTracker, "Mesh");
            meshObj.m_TextureIdx = textureIdx;

            meshes.push_back(meshObj);
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            processNode(node->mChildren[i], currentScene);
    };

    processNode(scene->mRootNode, scene);
    return meshes;
}

Mesh MeshFactory::Build_Mesh(
    VmaAllocator &allocator,
    std::deque<std::function<void(VmaAllocator)>> &deletionQueue,
    const vk::CommandBuffer &commandBuffer,
    vk::Queue graphicsQueue,
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t> indices,
    ResourceTracker* allocTracker,
    const std::string& allocName
) {
    Mesh mesh{};
    mesh.m_IndexCount = static_cast<uint32_t>(indices.size());

    // Vertex Buffer
    BufferInfo vertexStaging = m_MeshBuffer->CreateMapped(allocator, vertices.size() * sizeof(Vertex),
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
        allocTracker, "VertexStaging");
    Buffer::UploadData(vertexStaging, vertices.data(), vertices.size() * sizeof(Vertex));

    mesh.m_VertexBufferInfo = m_MeshBuffer->CreateMapped(allocator,
        vertices.size() * sizeof(Vertex),
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO, 0, allocTracker, allocName);

    Buffer::Copy(vertexStaging.m_Buffer, vertexStaging.m_AllocInfo,
        mesh.m_VertexBufferInfo.m_Buffer, mesh.m_VertexBufferInfo.m_AllocInfo,
        graphicsQueue, commandBuffer);

    Buffer::Destroy(allocator, vertexStaging.m_Buffer, vertexStaging.m_Allocation, allocTracker);

    // Index Buffer
    BufferInfo indexStaging = m_MeshBuffer->CreateMapped(allocator, indices.size() * sizeof(indices[0]),
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
        allocTracker, "IndexStaging");
    Buffer::UploadData(indexStaging, indices.data(), indices.size() * sizeof(indices[0]));

    mesh.m_IndexBufferInfo = m_MeshBuffer->CreateMapped(allocator,
        indices.size() * sizeof(indices[0]),
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO, 0, allocTracker, "IndexBuffer");

    Buffer::Copy(indexStaging.m_Buffer, indexStaging.m_AllocInfo,
        mesh.m_IndexBufferInfo.m_Buffer, mesh.m_IndexBufferInfo.m_AllocInfo,
        graphicsQueue, commandBuffer);

    Buffer::Destroy(allocator, indexStaging.m_Buffer, indexStaging.m_Allocation, allocTracker);

    // Schedule GPU buffer deletion
    auto vb = mesh.m_VertexBufferInfo.m_Buffer;
    auto va = mesh.m_VertexBufferInfo.m_Allocation;
    auto ib = mesh.m_IndexBufferInfo.m_Buffer;
    auto ia = mesh.m_IndexBufferInfo.m_Allocation;

    deletionQueue.emplace_back([vb, va, ib, ia, allocTracker](VmaAllocator alloc) {
        Buffer::Destroy(alloc, vb, va, allocTracker);
        Buffer::Destroy(alloc, ib, ia, allocTracker);
    });

    return mesh;
}
