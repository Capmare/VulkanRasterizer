//
// Created by capma on 8/7/2025.
//

#include "DescriptorSets.h"


#include <ranges>

#include "Factories/MeshFactory.h"
#include "Structs/Lights.h"
#include "Structs/UBOStructs.h"

void DescriptorSets::CreateFrameDescriptorSet(const vk::DescriptorSetLayout &FrameLayout,
                                              const std::tuple<vk::ImageView, vk::ImageView, vk::ImageView> &
                                              ColorImageViews, const vk::ImageView &DepthImageView,
                                              const BufferInfo &UniformBufferInfo, const BufferInfo &ShadowBufferInfo,
                                              const vk::ImageView &ShadowImageView) {
    m_FrameDescriptorSets.release();

    vk::DescriptorSetLayout FrameDescriptorSetLayoutArr[] = {FrameLayout, FrameLayout};

    vk::DescriptorSetAllocateInfo FrameAllocInfo{};
    // Allocate descriptor set
    FrameAllocInfo.descriptorPool = *m_DescriptorPool;
    FrameAllocInfo.descriptorSetCount = 2;
    FrameAllocInfo.pSetLayouts = FrameDescriptorSetLayoutArr;

    m_FrameDescriptorSets = std::make_unique<vk::raii::DescriptorSets>(m_Device, FrameAllocInfo);

    // Descriptor buffer info (uniform buffer)
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = UniformBufferInfo.m_Buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(MVP);

    vk::DescriptorImageInfo DiffuseImageInfo{};
    DiffuseImageInfo.imageLayout = vk::ImageLayout::eReadOnlyOptimal;
    DiffuseImageInfo.imageView = std::get<0>(ColorImageViews);
    DiffuseImageInfo.sampler = nullptr;

    vk::DescriptorImageInfo NormalImageInfo{};
    NormalImageInfo.imageLayout = vk::ImageLayout::eReadOnlyOptimal;
    NormalImageInfo.imageView = std::get<1>(ColorImageViews);
    NormalImageInfo.sampler = nullptr;

    vk::DescriptorImageInfo MaterialImageInfo{};
    MaterialImageInfo.imageLayout = vk::ImageLayout::eReadOnlyOptimal;
    MaterialImageInfo.imageView = std::get<2>(ColorImageViews);
    MaterialImageInfo.sampler = nullptr;

    vk::DescriptorImageInfo DepthImageInfo{};
    DepthImageInfo.imageLayout = vk::ImageLayout::eReadOnlyOptimal;
    DepthImageInfo.imageView = DepthImageView;
    DepthImageInfo.sampler = nullptr;

    vk::DescriptorBufferInfo shadowBufferInfo{};
    shadowBufferInfo.buffer = ShadowBufferInfo.m_Buffer;
    shadowBufferInfo.offset = 0;
    shadowBufferInfo.range = sizeof(ShadowMVP);

    vk::DescriptorImageInfo ShadowImageInfo{};
    ShadowImageInfo.imageLayout = vk::ImageLayout::eReadOnlyOptimal;
    ShadowImageInfo.imageView = ShadowImageView;
    ShadowImageInfo.sampler = nullptr;

    for (auto &ds: *m_FrameDescriptorSets) {
        vk::WriteDescriptorSet writeInfo{};
        writeInfo.dstSet = ds;
        writeInfo.descriptorType = vk::DescriptorType::eUniformBuffer;
        writeInfo.descriptorCount = static_cast<uint32_t>(1);
        writeInfo.pImageInfo = nullptr;
        writeInfo.pBufferInfo = &bufferInfo;
        writeInfo.dstBinding = 0;

        vk::WriteDescriptorSet textureDiffuse{};
        textureDiffuse.dstSet = ds;
        textureDiffuse.descriptorType = vk::DescriptorType::eSampledImage;
        textureDiffuse.descriptorCount = static_cast<uint32_t>(1);
        textureDiffuse.pImageInfo = &DiffuseImageInfo;
        textureDiffuse.dstBinding = 1;
        textureDiffuse.pBufferInfo = nullptr;

        vk::WriteDescriptorSet textureNormal{};
        textureNormal.dstSet = ds;
        textureNormal.descriptorType = vk::DescriptorType::eSampledImage;
        textureNormal.descriptorCount = static_cast<uint32_t>(1);
        textureNormal.pImageInfo = &NormalImageInfo;
        textureNormal.dstBinding = 2;
        textureNormal.pBufferInfo = nullptr;

        vk::WriteDescriptorSet textureMaterial{};
        textureMaterial.dstSet = ds;
        textureMaterial.descriptorType = vk::DescriptorType::eSampledImage;
        textureMaterial.descriptorCount = static_cast<uint32_t>(1);
        textureMaterial.pImageInfo = &MaterialImageInfo;
        textureMaterial.dstBinding = 3;
        textureMaterial.pBufferInfo = nullptr;

        vk::WriteDescriptorSet textureDepth{};
        textureDepth.dstSet = ds;
        textureDepth.descriptorType = vk::DescriptorType::eSampledImage;
        textureDepth.descriptorCount = static_cast<uint32_t>(1);
        textureDepth.pImageInfo = &DepthImageInfo;
        textureDepth.dstBinding = 4;
        textureDepth.pBufferInfo = nullptr;

        vk::WriteDescriptorSet ShadowWriteInfo{};
        ShadowWriteInfo.dstSet = ds;
        ShadowWriteInfo.descriptorType = vk::DescriptorType::eUniformBuffer;
        ShadowWriteInfo.descriptorCount = static_cast<uint32_t>(1);
        ShadowWriteInfo.pImageInfo = nullptr;
        ShadowWriteInfo.pBufferInfo = &shadowBufferInfo;
        ShadowWriteInfo.dstBinding = 5;

        vk::WriteDescriptorSet textureShadow{};
        textureShadow.dstSet = ds;
        textureShadow.descriptorType = vk::DescriptorType::eSampledImage;
        textureShadow.descriptorCount = static_cast<uint32_t>(1);
        textureShadow.pImageInfo = &ShadowImageInfo;
        textureShadow.dstBinding = 6;
        textureShadow.pBufferInfo = nullptr;

        m_Device.updateDescriptorSets({
                                          writeInfo, textureDiffuse, textureNormal, textureMaterial, textureDepth,
                                          ShadowWriteInfo, textureShadow
                                      }, nullptr);
    }
}

void DescriptorSets::CreateDescriptorPool() {
    vk::DescriptorPoolSize UboPoolSize{};
    UboPoolSize.type = vk::DescriptorType::eUniformBuffer;
    UboPoolSize.descriptorCount = 4;

    vk::DescriptorPoolSize SamplerPoolSize{};
    SamplerPoolSize.type = vk::DescriptorType::eSampler;
    SamplerPoolSize.descriptorCount = 2;

    vk::DescriptorPoolSize TexturesPoolSize{};
    TexturesPoolSize.type = vk::DescriptorType::eSampledImage;
    TexturesPoolSize.descriptorCount = 14;

    vk::DescriptorPoolSize PoolSizeArr[] = {UboPoolSize, SamplerPoolSize, TexturesPoolSize};

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.maxSets = 4;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = PoolSizeArr;
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

    m_Device.waitIdle();
    m_DescriptorPool.release();
    m_DescriptorPool = std::make_unique<vk::raii::DescriptorPool>(m_Device.createDescriptorPool(poolInfo));
}

void DescriptorSets::CreateGlobalDescriptorSet(
    const vk::DescriptorSetLayout &GlobalLayout, const vk::Sampler &Sampler,
    const std::pair<BufferInfo, uint32_t> &PointLights, const std::pair<BufferInfo, uint32_t> &DirectionalLights,
    const std::vector<ImageResource> &ImageResources,
    const std::vector<vk::ImageView> &SwapchainImageViews,
    const vk::Sampler &ShadowSampler
) {
    m_GlobalDescriptorSets.release();

    // global descriptor set
    vk::DescriptorSetLayout GlobalDescriptorSetLayoutArr[] = {GlobalLayout, GlobalLayout};

    vk::DescriptorSetAllocateInfo GlobalAllocInfo{};
    GlobalAllocInfo.descriptorPool = *m_DescriptorPool;
    GlobalAllocInfo.descriptorSetCount = 2;
    GlobalAllocInfo.pSetLayouts = GlobalDescriptorSetLayoutArr;

    auto oldDescriptorSet = std::move(m_GlobalDescriptorSets);
    m_GlobalDescriptorSets.reset();
    m_GlobalDescriptorSets = std::make_unique<vk::raii::DescriptorSets>(m_Device, GlobalAllocInfo);

    vk::DescriptorImageInfo SamplerInfo{};
    SamplerInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    SamplerInfo.imageView = VK_NULL_HANDLE;
    SamplerInfo.sampler = Sampler;

    vk::DescriptorBufferInfo LightBufferInfo{};
    LightBufferInfo.buffer = PointLights.first.m_Buffer;
    LightBufferInfo.offset = 0;
    LightBufferInfo.range = sizeof(PointLight) * PointLights.second;

    vk::DescriptorBufferInfo DirectionalLightBufferInfo{};
    DirectionalLightBufferInfo.buffer = DirectionalLights.first.m_Buffer;
    DirectionalLightBufferInfo.offset = 0;
    DirectionalLightBufferInfo.range = sizeof(DirectionalLight) * DirectionalLights.second;

    vk::DescriptorImageInfo ShadowSamplerInfo{};
    ShadowSamplerInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    ShadowSamplerInfo.imageView = VK_NULL_HANDLE;
    ShadowSamplerInfo.sampler = ShadowSampler;

    for (auto &ds: *m_GlobalDescriptorSets) {
        std::vector<vk::WriteDescriptorSet> descriptorWrites{};
        descriptorWrites.emplace_back();

        descriptorWrites[0].dstSet = ds;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = vk::DescriptorType::eSampler;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &SamplerInfo;

        std::vector<vk::DescriptorImageInfo> DescriptorImgInfos{};
        for (auto [i, img]: ImageResources | std::ranges::views::enumerate) {
            vk::DescriptorImageInfo descriptorImageInfo{};
            descriptorImageInfo.imageLayout = img.imageLayout;
            descriptorImageInfo.imageView = SwapchainImageViews[i];

            DescriptorImgInfos.emplace_back(descriptorImageInfo);
        }

        descriptorWrites.emplace_back();
        descriptorWrites[1].dstSet = ds;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = vk::DescriptorType::eSampledImage;
        descriptorWrites[1].descriptorCount = static_cast<uint32_t>(DescriptorImgInfos.size());
        descriptorWrites[1].pImageInfo = DescriptorImgInfos.data();

        descriptorWrites.emplace_back();
        descriptorWrites[2].dstSet = ds;
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &LightBufferInfo;

        descriptorWrites.emplace_back();
        descriptorWrites[3].dstSet = ds;
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pBufferInfo = &DirectionalLightBufferInfo;

        descriptorWrites.emplace_back();
        descriptorWrites[4].dstSet = ds;
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = vk::DescriptorType::eSampler;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pImageInfo = &ShadowSamplerInfo;

        m_Device.updateDescriptorSets(descriptorWrites, nullptr);
    }
}
