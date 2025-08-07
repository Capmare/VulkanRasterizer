//
// Created by capma on 8/7/2025.
//

#ifndef SHADOWPASS_H
#define SHADOWPASS_H
#include <functional>

#include "ResourceTracker.h"
#include <vulkan/vulkan_raii.hpp>

#include "Factories/ImageFactory.h"
#include "Factories/MeshFactory.h"
#include "Factories/PipelineFactory.h"

class ShadowPass {
public:
    ShadowPass(const vk::raii::Device& device, const std::vector<std::unique_ptr<vk::raii::CommandBuffer>>& CommandBuffer ) : m_Device(device), m_CommandBuffer(CommandBuffer) {
        m_PipelineFactory = std::make_unique<PipelineFactory>(m_Device);
    } ;
    virtual ~ShadowPass() = default;

    ShadowPass(const ShadowPass&) = delete;
    ShadowPass(ShadowPass&&) noexcept = delete;
    ShadowPass& operator=(const ShadowPass&) = delete;
    ShadowPass& operator=(ShadowPass&&) noexcept = delete;

    void CreatePipeline(uint32_t shadowMapCount, vk::Format format);

    void DoPass(uint32_t CurrentFrame, uint32_t width, uint32_t height) const;
    void CreateShadowResources(VmaAllocator allocator, std::deque<std::function<void(VmaAllocator)>> &deletionQueue, ResourceTracker *tracker, uint32_t
                               width, uint32_t height);

    vk::PipelineLayout m_PipelineLayout;
    std::vector<vk::DescriptorSet> m_DescriptorSets;

    vk::ImageView GetImageView() const { return **m_ShadowImageView; };

    void SetMeshes(const std::vector<Mesh>& Meshes) { m_Meshes = Meshes; };
private:

    const vk::raii::Device& m_Device;
    const std::vector<std::unique_ptr<vk::raii::CommandBuffer>>& m_CommandBuffer;


    std::unique_ptr<ImageResource> m_ShadowImageResource;
    std::unique_ptr<vk::raii::ImageView> m_ShadowImageView;
    std::unique_ptr<vk::raii::RenderPass> m_RenderPass;
    std::unique_ptr<vk::raii::Framebuffer> m_Framebuffer;
    std::unique_ptr<vk::raii::Sampler> m_ShadowSampler;


    std::vector<Mesh> m_Meshes;
    std::unique_ptr<vk::raii::Pipeline> m_Pipeline;
    std::unique_ptr<PipelineFactory> m_PipelineFactory;
};



#endif //SHADOWPASS_H
