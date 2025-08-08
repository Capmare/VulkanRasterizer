//
// Created by capma on 8/6/2025.
//

#ifndef GBUFFERPASS_H
#define GBUFFERPASS_H
#include <complex.h>
#include <deque>
#include <functional>
#include <memory>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include "Factories/ImageFactory.h"
#include "Factories/MeshFactory.h"

class PipelineFactory;

class GBufferPass {

public:


    GBufferPass(const vk::raii::Device& Device, const std::vector<std::unique_ptr<vk::raii::CommandBuffer>>& CommandBuffer);
    virtual ~GBufferPass() = default;

    void PrepareImagesForRead(uint32_t CurrentFrame);

    void WindowResizeShiftLayout(const vk::raii::CommandBuffer &command_buffer);

    GBufferPass(const GBufferPass&) = delete;
    GBufferPass(GBufferPass&&) noexcept = delete;
    GBufferPass& operator=(const GBufferPass&) = delete;
    GBufferPass& operator=(GBufferPass&&) noexcept = delete;

    void CreateGBuffer(VmaAllocator Allocator, std::deque<std::function<void(VmaAllocator)>> &VmaAllocatorsDeletionQueue, ResourceTracker *
                       AllocationTracker, uint32_t width, uint32_t height);
    void DoPass(vk::ImageView DepthImageView, uint32_t CurrentFrame, uint32_t width, uint32_t height);

    void ShiftLayout(const vk::raii::CommandBuffer & command_buffer);

    void SetMeshes(const std::vector<Mesh>& Meshes) { m_Meshes = Meshes; };
    void CreatePipeline(uint32_t ImageResourceSize, const vk::Format &DepthFormat);

	void RecreateGBuffer(VmaAllocator Allocator, std::deque<std::function<void(VmaAllocator)>> &VmaAllocatorsDeletionQueue, ResourceTracker *
	                     AllocationTracker, uint32_t width, uint32_t height);

	// returns diffuse normal material
	std::tuple<VkImageView,VkImageView,VkImageView> GetImageViews();
    std::vector<vk::DescriptorSet> m_DescriptorSets;
    vk::PipelineLayout m_PipelineLayout;

private:
	void CreateModules();

	const vk::raii::Device& m_Device;
    const std::vector<std::unique_ptr<vk::raii::CommandBuffer>>& m_CommandBuffer;

	std::unique_ptr<PipelineFactory> m_GBufferPipelineFactory{};
	std::unique_ptr<vk::raii::Pipeline> m_GBufferPipeline{};

	std::vector<vk::raii::ShaderModule> m_GBufferShaderModules{};


    std::vector<Mesh> m_Meshes;

    // G-buffer images
    ImageResource m_GBufferDiffuse;
    ImageResource m_GBufferNormals;
    ImageResource m_GBufferMaterial;

    VkImageView m_GBufferDiffuseView{};
    VkImageView m_GBufferNormalsView{};
    VkImageView m_GBufferMaterialView{};
};



#endif //GBUFFERPASS_H
