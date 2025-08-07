//
// Created by capma on 8/6/2025.
//

#ifndef COLORPASS_H
#define COLORPASS_H
#include "Buffer.h"
#include "Factories/PipelineFactory.h"
#include "glm/glm.hpp"

struct DirectionalLight;
struct PointLight;

class ColorPass {

public:

	ColorPass(const vk::raii::Device &Device, const vk::PipelineLayout &PipelineLayout,
		  const std::vector<std::unique_ptr<vk::raii::CommandBuffer>> &CommandBuffer,
		  const std::pair<std::vector<DirectionalLight>, std::vector<PointLight>> &LightData,
		  const std::pair<vk::Format, vk::Format> &ColorDepthFormat);

    virtual ~ColorPass() = default;

    ColorPass(const ColorPass&) = delete;
    ColorPass(ColorPass&&) noexcept = delete;
    ColorPass& operator=(const ColorPass&) = delete;
    ColorPass& operator=(ColorPass&&) noexcept = delete;

	void DoPass(const std::vector<vk::raii::ImageView> &ImageView, int CurrentFrame, glm::uint32_t imageIndex, int width, int height) const;

    std::vector<vk::DescriptorSet> m_DescriptorSets;

	std::pair<vk::Format, vk::Format> m_Format{};

private:
    void CreateGraphicsPipeline();
	void CreateModules();

	const vk::raii::Device& m_Device;
	std::unique_ptr<vk::raii::Pipeline> m_GraphicsPipeline{};
	std::unique_ptr<PipelineFactory> m_GraphicsPipelineFactory{};
    const vk::PipelineLayout &m_PipelineLayout;
    const std::vector<std::unique_ptr<vk::raii::CommandBuffer>>& m_CommandBuffer;

	std::vector<vk::raii::ShaderModule> m_ColorShaderModules{};

	std::pair<std::vector<DirectionalLight>, std::vector<PointLight>>  m_LightData{};





};



#endif //COLORPASS_H
