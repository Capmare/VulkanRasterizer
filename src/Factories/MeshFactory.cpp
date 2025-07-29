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
    const std::string& path,
    VmaAllocator& allocator,
    std::deque<std::function<void(VmaAllocator)>>& deletionQueue,
    const vk::CommandBuffer& commandBuffer,
    const vk::raii::Queue& graphicsQueue,
    vk::raii::Device& device,
    const vk::raii::CommandPool& cmdPool,
    std::vector<ImageResource>& textures,
    std::vector<vk::ImageView>& textureImageViews,
    ResourceTracker* allocTracker
) {
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
            aiMesh* ai_mesh = currentScene->mMeshes[node->mMeshes[i]];
            aiMaterial* materialPtr = currentScene->mMaterials[ai_mesh->mMaterialIndex];

            // Process vertices
            std::vector<Vertex> vertices;
            vertices.reserve(ai_mesh->mNumVertices);
            aiMatrix4x4 transform = currentScene->mRootNode->mTransformation;

            for (unsigned int v = 0; v < ai_mesh->mNumVertices; ++v) {
                Vertex vert{};
                aiVector3D aiPosition = transform * ai_mesh->mVertices[v];
                vert.pos = glm::vec3(aiPosition.x, aiPosition.y, aiPosition.z);

                if (ai_mesh->HasTextureCoords(0)) {
                    vert.texCoord = glm::vec2(ai_mesh->mTextureCoords[0][v].x, ai_mesh->mTextureCoords[0][v].y);
                }
                if (ai_mesh->HasNormals()) {
                    vert.normal = glm::vec3(ai_mesh->mNormals[v].x,
                                            ai_mesh->mNormals[v].y,
                                            ai_mesh->mNormals[v].z);
                }

                if (ai_mesh->HasTangentsAndBitangents()) {
                    vert.tangent = glm::vec3(ai_mesh->mTangents[v].x,
                                             ai_mesh->mTangents[v].y,
                                             ai_mesh->mTangents[v].z);
                    vert.bitangent = glm::vec3(ai_mesh->mBitangents[v].x,
                                               ai_mesh->mBitangents[v].y,
                                               ai_mesh->mBitangents[v].z);
                }

                vertices.push_back(vert);
            }

            // Process indices
            std::vector<uint32_t> indices;
            for (unsigned int f = 0; f < ai_mesh->mNumFaces; ++f) {
                const aiFace& face = ai_mesh->mFaces[f];
                for (unsigned int j = 0; j < face.mNumIndices; ++j)
                    indices.push_back(face.mIndices[j]);
            }

            // Load material textures
            Material material{};
            auto loadTextureIndex = [&](aiMaterial* mat, aiTextureType type, vk::Format format = vk::Format::eR8G8B8A8Srgb) -> int {
                if (mat->GetTextureCount(type) > 0) {
                    aiString texPath;
                    if (mat->GetTexture(type, 0, &texPath) == AI_SUCCESS) {
                        return LoadTextureGeneric(scene, texPath.C_Str(), baseDir, textureCache,
                                                  textures, textureImageViews,
                                                  allocator, deletionQueue, device, cmdPool, graphicsQueue, allocTracker, format);
                    }
                }
                return -1;
            };

            material.diffuseIdx   = loadTextureIndex(materialPtr, aiTextureType_DIFFUSE);
            material.normalIdx    = loadTextureIndex(materialPtr, aiTextureType_NORMALS, vk::Format::eR8G8B8A8Unorm);
            material.metallicIdx  = loadTextureIndex(materialPtr, aiTextureType_METALNESS);
            material.roughnessIdx = loadTextureIndex(materialPtr, aiTextureType_DIFFUSE_ROUGHNESS);
            material.aoIdx        = loadTextureIndex(materialPtr, aiTextureType_AMBIENT_OCCLUSION);
            material.emissiveIdx  = loadTextureIndex(materialPtr, aiTextureType_EMISSIVE);

            // Build GPU buffers for mesh
            Mesh meshObj = Build_Mesh(
                allocator, deletionQueue, commandBuffer, graphicsQueue,
                vertices, indices, allocTracker, "Mesh");
            meshObj.m_Material = material;

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



int MeshFactory::LoadTextureGeneric(
    const aiScene* scene,
    const std::string& texPathStr,
    const std::filesystem::path& baseDir,
    std::unordered_map<std::string, uint32_t>& textureCache,
    std::vector<ImageResource>& textures,
    std::vector<vk::ImageView>& textureImageViews,
    VmaAllocator& allocator,
    std::deque<std::function<void(VmaAllocator)>>& deletionQueue,
    vk::raii::Device& device,
    const vk::raii::CommandPool& cmdPool,
    const vk::raii::Queue& graphicsQueue,
    ResourceTracker* allocTracker,
    vk::Format format = vk::Format::eR8G8B8A8Srgb
) {
    std::string fullPath = (baseDir / texPathStr).string();

    if (textureCache.contains(fullPath)) {
        return textureCache[fullPath];
    }

    auto texture = ImageFactory::LoadTexture(
        m_MeshBuffer.get(), fullPath, device, allocator, cmdPool,
        graphicsQueue, format, vk::ImageAspectFlagBits::eColor, allocTracker);

    textures.emplace_back(texture);

    auto imageView = ImageFactory::CreateImageView(
        device, texture.image, format,
        vk::ImageAspectFlagBits::eColor, allocTracker,
        "textureImageView: " + fullPath);

    textureImageViews.emplace_back(imageView);
    int textureIdx = static_cast<int>(textures.size() - 1);
    textureCache[fullPath] = textureIdx;

    // Cleanup function
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

    return textureIdx;
}

