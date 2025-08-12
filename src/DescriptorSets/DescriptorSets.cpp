//
// Created by capma on 8/7/2025.
//

#include "DescriptorSets.h"


#include <complex.h>
#include <complex.h>
#include <ranges>

#include "Factories/MeshFactory.h"
#include "Structs/Lights.h"
#include "Structs/UBOStructs.h"


void DescriptorSets::CreateFrameDescriptorSet(
    const vk::DescriptorSetLayout &FrameLayout,
    const std::tuple<vk::ImageView, vk::ImageView, vk::ImageView> &ColorImageViews,
    const vk::ImageView &DepthImageView,
    const BufferInfo &UniformBufferInfo,
    const BufferInfo &ShadowBufferInfo,
    const std::vector<vk::ImageView> &ShadowImageViews,
    const vk::ImageView& CubemapImage
    )
{

    // Allocate two descriptor sets
    vk::DescriptorSetLayout FrameDescriptorSetLayoutArr[] = { FrameLayout, FrameLayout };

    vk::DescriptorSetAllocateInfo FrameAllocInfo{};
    FrameAllocInfo.descriptorPool = m_DescriptorPool;
    FrameAllocInfo.descriptorSetCount = 2;
    FrameAllocInfo.pSetLayouts = FrameDescriptorSetLayoutArr;

    auto dev = *m_Device;
    m_FrameDescriptorSets = dev.allocateDescriptorSets(FrameAllocInfo);

    // Descriptor buffer info
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = UniformBufferInfo.m_Buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(MVP);

    vk::DescriptorImageInfo DiffuseImageInfo{};
    DiffuseImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    DiffuseImageInfo.imageView = std::get<0>(ColorImageViews);
    DiffuseImageInfo.sampler = nullptr;

    vk::DescriptorImageInfo NormalImageInfo{};
    NormalImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    NormalImageInfo.imageView = std::get<1>(ColorImageViews);
    NormalImageInfo.sampler = nullptr;

    vk::DescriptorImageInfo MaterialImageInfo{};
    MaterialImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    MaterialImageInfo.imageView = std::get<2>(ColorImageViews);
    MaterialImageInfo.sampler = nullptr;

    vk::DescriptorImageInfo DepthImageInfo{};
    DepthImageInfo.imageLayout = vk::ImageLayout::eDepthReadOnlyOptimal;
    DepthImageInfo.imageView = DepthImageView;
    DepthImageInfo.sampler = nullptr;

    vk::DescriptorBufferInfo shadowBufferInfo{};
    shadowBufferInfo.buffer = ShadowBufferInfo.m_Buffer;
    shadowBufferInfo.offset = 0;
    shadowBufferInfo.range = sizeof(ShadowMVP);

    vk::DescriptorImageInfo CubemapImageInfo{};
    CubemapImageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    CubemapImageInfo.imageView = CubemapImage;
    CubemapImageInfo.sampler = nullptr;

    std::vector<vk::DescriptorImageInfo> shadowImageInfos;
    shadowImageInfos.reserve(ShadowImageViews.size());
    for (const auto& view : ShadowImageViews) {
        vk::DescriptorImageInfo info{};
        info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        info.imageView = view;
        info.sampler = nullptr;
        shadowImageInfos.push_back(info);
    }

    for (auto& ds : m_FrameDescriptorSets) {
        std::vector<vk::WriteDescriptorSet> writes;

        // Uniform buffer
        vk::WriteDescriptorSet writeUBO{};
        writeUBO.dstSet = ds;
        writeUBO.dstBinding = 0;
        writeUBO.dstArrayElement = 0;
        writeUBO.descriptorCount = 1;
        writeUBO.descriptorType = vk::DescriptorType::eUniformBuffer;
        writeUBO.pBufferInfo = &bufferInfo;
        writes.push_back(writeUBO);

        // Diffuse texture
        vk::WriteDescriptorSet writeDiffuse{};
        writeDiffuse.dstSet = ds;
        writeDiffuse.dstBinding = 1;
        writeDiffuse.dstArrayElement = 0;
        writeDiffuse.descriptorCount = 1;
        writeDiffuse.descriptorType = vk::DescriptorType::eSampledImage;
        writeDiffuse.pImageInfo = &DiffuseImageInfo;
        writes.push_back(writeDiffuse);

        // Normal texture
        vk::WriteDescriptorSet writeNormal{};
        writeNormal.dstSet = ds;
        writeNormal.dstBinding = 2;
        writeNormal.dstArrayElement = 0;
        writeNormal.descriptorCount = 1;
        writeNormal.descriptorType = vk::DescriptorType::eSampledImage;
        writeNormal.pImageInfo = &NormalImageInfo;
        writes.push_back(writeNormal);

        // Material texture
        vk::WriteDescriptorSet writeMaterial{};
        writeMaterial.dstSet = ds;
        writeMaterial.dstBinding = 3;
        writeMaterial.dstArrayElement = 0;
        writeMaterial.descriptorCount = 1;
        writeMaterial.descriptorType = vk::DescriptorType::eSampledImage;
        writeMaterial.pImageInfo = &MaterialImageInfo;
        writes.push_back(writeMaterial);

        // Depth texture
        vk::WriteDescriptorSet writeDepth{};
        writeDepth.dstSet = ds;
        writeDepth.dstBinding = 4;
        writeDepth.dstArrayElement = 0;
        writeDepth.descriptorCount = 1;
        writeDepth.descriptorType = vk::DescriptorType::eSampledImage;
        writeDepth.pImageInfo = &DepthImageInfo;
        writes.push_back(writeDepth);

        // Shadow UBO
        vk::WriteDescriptorSet writeShadowUBO{};
        writeShadowUBO.dstSet = ds;
        writeShadowUBO.dstBinding = 5;
        writeShadowUBO.dstArrayElement = 0;
        writeShadowUBO.descriptorCount = 1;
        writeShadowUBO.descriptorType = vk::DescriptorType::eUniformBuffer;
        writeShadowUBO.pBufferInfo = &shadowBufferInfo;
        writes.push_back(writeShadowUBO);

        // Shadow texture array
        vk::WriteDescriptorSet writeShadowTextures{};
        writeShadowTextures.dstSet = ds;
        writeShadowTextures.dstBinding = 6;
        writeShadowTextures.dstArrayElement = 0;
        writeShadowTextures.descriptorCount = static_cast<uint32_t>(shadowImageInfos.size());
        writeShadowTextures.descriptorType = vk::DescriptorType::eSampledImage;
        writeShadowTextures.pImageInfo = shadowImageInfos.data();
        writes.push_back(writeShadowTextures);

        vk::WriteDescriptorSet writeCubemap{};
        writeCubemap.dstSet = ds;
        writeCubemap.dstBinding = 7;
        writeCubemap.dstArrayElement = 0;
        writeCubemap.descriptorCount = 1;
        writeCubemap.descriptorType = vk::DescriptorType::eSampledImage;
        writeCubemap.pImageInfo = &CubemapImageInfo;
        writes.push_back(writeCubemap);

        m_Device.updateDescriptorSets(writes, {});
    }
}



void DescriptorSets::CreateDescriptorPool(uint32_t DirectionalLights) {


    vk::DescriptorPoolSize UboPoolSize{};
    UboPoolSize.type = vk::DescriptorType::eUniformBuffer;
    UboPoolSize.descriptorCount = 4;

    vk::DescriptorPoolSize SamplerPoolSize{};
    SamplerPoolSize.type = vk::DescriptorType::eSampler;
    SamplerPoolSize.descriptorCount = 2;

    vk::DescriptorPoolSize TexturesPoolSize{};
    TexturesPoolSize.type = vk::DescriptorType::eSampledImage;
    TexturesPoolSize.descriptorCount = 14 + DirectionalLights;

    vk::DescriptorPoolSize PoolSizeArr[] = {UboPoolSize, SamplerPoolSize, TexturesPoolSize};

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.maxSets = 4;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = PoolSizeArr;
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;

    m_Device.waitIdle();
    auto dev = *m_Device;
    m_DescriptorPool = dev.createDescriptorPool(poolInfo);
}

void DescriptorSets::CreateGlobalDescriptorSet(
    const vk::DescriptorSetLayout &GlobalLayout, const vk::Sampler &Sampler,
    const std::pair<BufferInfo, uint32_t> &PointLights, const std::pair<BufferInfo, uint32_t> &DirectionalLights,
    const std::vector<ImageResource> &ImageResources,
    const std::vector<vk::ImageView> &SwapchainImageViews,
    const vk::Sampler &ShadowSampler
) {

    // global descriptor set
    vk::DescriptorSetLayout GlobalDescriptorSetLayoutArr[] = {GlobalLayout, GlobalLayout};

    vk::DescriptorSetAllocateInfo GlobalAllocInfo{};
    GlobalAllocInfo.descriptorPool = m_DescriptorPool;
    GlobalAllocInfo.descriptorSetCount = 2;
    GlobalAllocInfo.pSetLayouts = GlobalDescriptorSetLayoutArr;

    auto dev = *m_Device;
    m_GlobalDescriptorSets = dev.allocateDescriptorSets(GlobalAllocInfo);

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

    for (auto &ds: m_GlobalDescriptorSets) {
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
