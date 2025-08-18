//
// Created by capma on 8/6/2025.
//

#ifndef DEPTHPASS_H
#define DEPTHPASS_H
#include <memory>
#include <vector>

#include "Buffer.h"
#include "Factories/LogicalDeviceFactory.h"
#include "Factories/MeshFactory.h"
#include "Factories/PipelineFactory.h"

class DepthPass {
public:
    DepthPass(const vk::raii::Device& Device, std::vector<std::unique_ptr<vk::raii::CommandBuffer>>& CommandBuffer);

    void WindowResizeShiftLayout(const vk::raii::CommandBuffer &command_buffer);;
    virtual ~DepthPass() {

    };

    void ShiftLayout(const vk::raii::CommandBuffer & command_buffer);

    DepthPass(const DepthPass&) = delete;
    DepthPass(DepthPass&&) noexcept = delete;
    DepthPass& operator=(const DepthPass&) = delete;
    DepthPass& operator=(DepthPass&&) noexcept = delete;

    void DoPass(uint32_t CurrentFrame, uint32_t width, uint32_t height);

    void SetMeshes(const std::vector<Mesh>& Meshes) { m_Meshes = Meshes; };

    void CreatePipeline(uint32_t ImageResourceSize, const std::pair<vk::Format, vk::Format> &ColorAndDepthFormat);

	void CreateImage(VmaAllocator Allocator, ResourceTracker *AllocationTracker, const vk::Format &DepthFormat, uint32_t width, uint32_t
	                 height);

	void RecreateImage(VmaAllocator Allocator, ResourceTracker *AllocationTracker, const vk::Format &DepthFormat, uint32_t width, uint32_t
	                   height);

	void DestroyImages(VmaAllocator Allocator);
	std::vector<vk::DescriptorSet> m_DescriptorSets;
	vk::PipelineLayout m_PipelineLayout;

	ImageResource& GetImage() { return m_DepthImage; };
	[[nodiscard]] vk::ImageView GetImageView() const { return m_DepthImageView; };

	std::pair<vk::Format, vk::Format> GetFormat() const { return m_Format; };
private:
	void CreateModules();

    const vk::raii::Device& m_Device;
    const std::vector<std::unique_ptr<vk::raii::CommandBuffer>>& m_CommandBuffer;

	std::unique_ptr<PipelineFactory> m_DepthPipelineFactory{};

	std::vector<vk::raii::ShaderModule> m_DepthShaderModules{};

	std::unique_ptr<vk::raii::Pipeline> m_DepthPrepassPipeline{};

	std::vector<Mesh> m_Meshes;

	ImageResource m_DepthImage;
	vk::ImageView m_DepthImageView;

	std::pair<vk::Format, vk::Format> m_Format;

	VmaAllocator m_Allocator;
	ResourceTracker* m_AllocationTracker;
};



#endif //DEPTHPASS_H
