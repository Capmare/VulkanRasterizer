//
// Created by capma on 8/6/2025.
//

#include "ColorPass.h"

#include <iostream>
#include <vulkan/vulkan.hpp>

#include "Factories/ShaderFactory.h"
#include "Structs/Lights.h"

ColorPass::ColorPass(const vk::raii::Device &Device, const vk::PipelineLayout &PipelineLayout,
    const std::vector<std::unique_ptr<vk::raii::CommandBuffer>> &CommandBuffer,
    const std::pair<std::vector<DirectionalLight>, std::vector<PointLight>>& LightData,
    const std::pair<vk::Format, vk::Format>& ColorDepthFormat
    )   : m_Device(Device)
        , m_PipelineLayout(PipelineLayout)
        , m_CommandBuffer(CommandBuffer)
        , m_LightData(LightData)
        , m_Format(ColorDepthFormat)

{

    m_GraphicsPipelineFactory = std::make_unique<PipelineFactory>(Device);
    CreateModules();
    CreateGraphicsPipeline();
}

void ColorPass::DoPass(const std::vector<vk::raii::ImageView>& ImageView ,int CurrentFrame, glm::uint32_t imageIndex, int width, int height) const {
    vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    vk::Rect2D scissor{{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};

    vk::RenderingAttachmentInfo colorAttachment{};
    colorAttachment.setImageView(ImageView[imageIndex]);
    colorAttachment.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachment.setClearValue(vk::ClearValue({0.5f, 0.0f, 0.25f, 1.0f}));

    vk::RenderingInfo renderInfo{};
    renderInfo.setRenderArea(scissor);
    renderInfo.setLayerCount(1);
    renderInfo.setColorAttachments(colorAttachment);

    m_CommandBuffer[CurrentFrame]->beginRendering(renderInfo);
    m_CommandBuffer[CurrentFrame]->bindPipeline(vk::PipelineBindPoint::eGraphics, *m_GraphicsPipeline);

    m_CommandBuffer[CurrentFrame]->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PipelineLayout, 0, m_DescriptorSets, {});
    m_CommandBuffer[CurrentFrame]->setViewport(0, viewport);
    m_CommandBuffer[CurrentFrame]->setScissor(0, scissor);
    m_CommandBuffer[CurrentFrame]->draw(3, 1, 0, 0);
    m_CommandBuffer[CurrentFrame]->endRendering();
}


void ColorPass::CreateGraphicsPipeline() {
      // Depth stencil
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = vk::CompareOp::eEqual;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blend attachment
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    colorBlendAttachment.blendEnable = VK_FALSE;

    // Multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

    // Rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Input assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Vertex input info
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.vertexBindingDescriptionCount = 0;

    using LightSpecPair = std::pair<uint32_t, uint32_t>;
    LightSpecPair specData{
        static_cast<uint32_t>(m_LightData.second.size()),
        static_cast<uint32_t>(m_LightData.first.size())
    };

    // Map entries
    std::array<vk::SpecializationMapEntry, 2> specMapEntries = {
        vk::SpecializationMapEntry{2, offsetof(LightSpecPair, first), sizeof(uint32_t)},  // Point lights
        vk::SpecializationMapEntry{3, offsetof(LightSpecPair, second), sizeof(uint32_t)} // Directional lights
    };

    // Specialization info
    vk::SpecializationInfo specializationInfo{};
    specializationInfo.mapEntryCount = static_cast<uint32_t>(specMapEntries.size());
    specializationInfo.pMapEntries = specMapEntries.data();
    specializationInfo.dataSize = sizeof(LightSpecPair);
    specializationInfo.pData = &specData;

    // Shader stages
    vk::PipelineShaderStageCreateInfo vertexStageInfo{};
    vertexStageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
    vertexStageInfo.setModule(*m_ColorShaderModules[0]);
    vertexStageInfo.setPName("main");

    vk::PipelineShaderStageCreateInfo fragmentStageInfo{};
    fragmentStageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
    fragmentStageInfo.setModule(*m_ColorShaderModules[1]);
    fragmentStageInfo.setPName("main");
    fragmentStageInfo.pSpecializationInfo = &specializationInfo;

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { vertexStageInfo, fragmentStageInfo };

    // Viewport state
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    // Formats
    vk::Format colorFormat = m_Format.first;
    vk::Format depthFormat = m_Format.second;

    // Build pipeline
    m_GraphicsPipeline = std::make_unique<vk::raii::Pipeline>(m_GraphicsPipelineFactory
        ->SetShaderStages(shaderStages)
        .SetVertexInput(vertexInputInfo)
        .SetInputAssembly(inputAssembly)
        .SetRasterizer(rasterizer)
        .SetMultisampling(multisampling)
        .SetColorBlendAttachments({ colorBlendAttachment })
        .SetViewportState(viewportState)
        .SetDynamicStates({ vk::DynamicState::eScissor, vk::DynamicState::eViewport })
        .SetDepthStencil(depthStencil)
        .SetLayout(m_PipelineLayout)
        .SetColorFormats({ colorFormat })
        .SetDepthFormat(depthFormat)
        .Build());
}

void ColorPass::CreateModules() {
    auto ShaderModules = ShaderFactory::Build_ShaderModules(m_Device, "shaders/shadervert.spv", "shaders/shaderfrag.spv");
    for (auto& shader : ShaderModules) {
        m_ColorShaderModules.emplace_back(std::move(shader));
    }
}
