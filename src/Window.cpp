#include "Window.h"


#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "Factories/ShaderFactory.h"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <ranges>

#include "Buffer.h"
#include "PhysicalDevicePicker.h"
#include "glm/gtx/transform.hpp"
#include "Structs/UBOStructs.h"


VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include "vmaFile.h"


static const glm::mat4 kClipBias = glm::mat4(
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, -1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.5f, 0.0f,
    0.0f, 0.0f, 0.5f, 1.0f
);


VulkanWindow::VulkanWindow(vk::raii::Context &context)
// Factories
    : m_InstanceFactory(std::make_unique<InstanceFactory>())
      , m_DebugMessengerFactory(std::make_unique<DebugMessengerFactory>())
      , m_LogicalDeviceFactory(std::make_unique<LogicalDeviceFactory>())
      , m_SwapChainFactory(std::make_unique<SwapChainFactory>())
      , m_Renderer(std::make_unique<Renderer>())

      // Vulkan
      , m_Context(std::move(context)) {
}

VulkanWindow::~VulkanWindow()
= default;

void VulkanWindow::FramebufferResizeCallback(GLFWwindow *window, int, int) {
    auto App = reinterpret_cast<VulkanWindow *>(glfwGetWindowUserPointer(window));
    App->m_bFrameBufferResized = true;
}

void VulkanWindow::MainLoop() {
    glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    SetupMouseCallback(m_Window);


    while (!glfwWindowShouldClose(m_Window)) {
        glfwPollEvents();

        const auto currentTime = glfwGetTime();
        const auto deltaTime = currentTime - lastFrameTime;
        lastFrameTime = currentTime;

        ProcessInput(m_Window, static_cast<float>(deltaTime));
        DrawFrame();

        m_GraphicsQueue->waitIdle();
    }
}

void VulkanWindow::Cleanup() {
    // Destroy depth image first (RAII)
    m_DepthImageFactory.reset();

    m_GBufferPass->DestroyImages(m_VmaAllocator);
    m_DepthPass->DestroyImages(m_VmaAllocator);

    // Destroy other buffers allocated with VMA manually
    while (!m_VmaAllocatorsDeletionQueue.empty()) {
        m_VmaAllocatorsDeletionQueue.back()(m_VmaAllocator);
        m_VmaAllocatorsDeletionQueue.pop_back();
    }


    m_ImageResource.clear();
    m_SwapChainImageViews.clear();

    m_AllocationTracker->PrintAllocations();
    m_AllocationTracker->PrintImageViews();


    vmaDestroyAllocator(m_VmaAllocator);

    m_SwapChainFactory.reset();
    m_SwapChain->clear();

    vkDestroySurfaceKHR(**m_Instance, m_Surface, nullptr);
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void VulkanWindow::UpdateUBO() {
    MVP ubo{};

    ubo.model = glm::translate(glm::mat4(1.0f), spawnPosition);
    ubo.view = m_Camera->GetViewMatrix();
    const float aspectRatio = m_CurrentScreenSize.x / m_CurrentScreenSize.y;
    ubo.proj = m_Camera->GetProjectionMatrix(aspectRatio);
    ubo.cameraPos = m_Camera->position;

    Buffer::UploadData(m_UniformBufferInfo, &ubo, sizeof(ubo));
}

void VulkanWindow::UpdateShadowUBO(uint32_t LightIdx) {
    // use the first directional light in your array
    glm::vec3 lightDir = glm::normalize(glm::vec3(m_DirectionalLights[LightIdx].Direction));

    glm::vec3 sceneMin, sceneMax;
    std::tie(sceneMin, sceneMax) = VulkanMath::ComputeSceneAABB(m_Meshes);
    glm::vec3 sceneCenter = 0.5f * (sceneMin + sceneMax);

    auto corners = VulkanMath::GetAABBCorners(sceneMin, sceneMax);

    float minProj = std::numeric_limits<float>::infinity();
    float maxProj = -std::numeric_limits<float>::infinity();
    for (auto &c: corners) {
        float p = glm::dot(c, lightDir);
        minProj = std::min(minProj, p);
        maxProj = std::max(maxProj, p);
    }

    float distanceToCenter = maxProj - glm::dot(sceneCenter, lightDir);
    glm::vec3 lightPos = sceneCenter - lightDir * distanceToCenter;

    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 up = (std::abs(glm::dot(lightDir, worldUp)) > 0.99f)
                       ? glm::vec3(0.0f, 0.0f, 1.0f)
                       : worldUp;

    glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, up);

    glm::vec3 minLS(std::numeric_limits<float>::infinity());
    glm::vec3 maxLS(-std::numeric_limits<float>::infinity());
    for (auto &c: corners) {
        glm::vec3 ls = glm::vec3(lightView * glm::vec4(c, 1.0f));
        minLS = glm::min(minLS, ls);
        maxLS = glm::max(maxLS, ls);
    }

    float nearPlane = -maxLS.z;
    float farPlane = -minLS.z;

    glm::mat4 lightProj = kClipBias * glm::ortho(
                              minLS.x, maxLS.x,
                              minLS.y, maxLS.y,
                              nearPlane, farPlane
                          );


    ShadowMVP smvp{};

    smvp.view = lightView;
    smvp.proj = lightProj;

    Buffer::UploadData(m_ShadowUBOBufferInfo, &smvp, sizeof(smvp));
}


void VulkanWindow::CreateSurface() {
    VkSurfaceKHR m_TempSurface;
    glfwCreateWindowSurface(**m_Instance, m_Window, nullptr, &m_TempSurface);
    m_Surface = m_TempSurface;
}

void VulkanWindow::SetupMouseCallback(GLFWwindow *window) {
    glfwSetWindowUserPointer(window, this);

    glfwSetCursorPosCallback(window, [](GLFWwindow *w, double xpos, double ypos) {
        if (const auto self = static_cast<VulkanWindow *>(glfwGetWindowUserPointer(w))) {
            self->mouse_callback(xpos, ypos);
        }
    });

    glfwSetMouseButtonCallback(m_Window, [](GLFWwindow *window, int button, int action, int mods) {
        if (const auto self = static_cast<VulkanWindow *>(glfwGetWindowUserPointer(window))) {
            self->mouse_button_callback(window, button, action, mods);
        }
    });
}

void VulkanWindow::mouse_button_callback(GLFWwindow *, int button, int action, int) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        firstMouse = true;
    }
}

void VulkanWindow::mouse_callback(double xpos, double ypos) {
    if (glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS)
        return;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = static_cast<float>(xpos - lastX);
    float yoffset = static_cast<float>(lastY - ypos); // reversed

    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    m_Camera->yaw += xoffset;
    m_Camera->pitch += yoffset;

    m_Camera->pitch = glm::clamp(m_Camera->pitch, -89.0f, 89.0f);
    m_Camera->UpdateTarget();
}


void VulkanWindow::ProcessInput(GLFWwindow *window, float deltaTime) {
    float velocity = cameraSpeed * deltaTime;

    glm::vec3 front = glm::normalize(m_Camera->target);
    glm::vec3 right = glm::normalize(glm::cross(front, m_Camera->up));
    glm::vec3 up = m_Camera->up;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        m_Camera->position += front * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        m_Camera->position -= front * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        m_Camera->position -= right * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        m_Camera->position += right * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        m_Camera->position += up * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        m_Camera->position -= up * velocity;
    }
}

void VulkanWindow::RenderToCubemap(const std::vector<vk::ShaderModule> &Shader, ImageResource &inImage,
                                   const vk::ImageView &inImageView, vk::Sampler sampler,
                                   ImageResource &outImage, std::array<vk::ImageView, 6> &outImageViews,
                                   const vk::Extent2D &renderArea, int inLayerCount = 1) {
    m_DescriptorSetFactory->ResetFactory();

    vk::raii::DescriptorSetLayout dsLayout = m_DescriptorSetFactory->AddBinding(
                0, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(1, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .Build();


    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4x4) * 2;
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &*dsLayout;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    vk::DescriptorSetAllocateInfo dsAllocInfo{};
    dsAllocInfo.descriptorPool = m_DescriptorSets->GetPool();
    dsAllocInfo.descriptorSetCount = 1;
    dsAllocInfo.pSetLayouts = &*dsLayout;

    auto vec = m_Device->allocateDescriptorSets(dsAllocInfo);
    vk::DescriptorSet ds{vec.front()};


    vk::DescriptorImageInfo samplerInfo{};
    samplerInfo.sampler = sampler;

    vk::DescriptorImageInfo imageViewInfo{};
    imageViewInfo.imageView = inImageView;
    imageViewInfo.imageLayout = inImage.imageLayout;


    vk::WriteDescriptorSet samplerDescriptorSet{};
    samplerDescriptorSet.descriptorCount = 1;
    samplerDescriptorSet.dstSet = ds;
    samplerDescriptorSet.dstBinding = 0;
    samplerDescriptorSet.dstArrayElement = 0;
    samplerDescriptorSet.pImageInfo = &samplerInfo;
    samplerDescriptorSet.descriptorType = vk::DescriptorType::eSampler;


    vk::WriteDescriptorSet imageViewDescriptorSet{};
    imageViewDescriptorSet.descriptorCount = 1;
    imageViewDescriptorSet.dstSet = ds;
    imageViewDescriptorSet.dstBinding = 1;
    imageViewDescriptorSet.dstArrayElement = 0;
    imageViewDescriptorSet.pImageInfo = &imageViewInfo;
    imageViewDescriptorSet.descriptorType = vk::DescriptorType::eSampledImage;

    m_Device->updateDescriptorSets({samplerDescriptorSet, imageViewDescriptorSet}, {});

    vk::raii::PipelineLayout pipelineLayout{std::move(m_Device->createPipelineLayout(pipelineLayoutCreateInfo))};

    std::unique_ptr<PipelineFactory> GraphicsPipelineFactory = std::make_unique<PipelineFactory>(*m_Device);

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

    // Shader stages
    vk::PipelineShaderStageCreateInfo vertexStageInfo{};
    vertexStageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
    vertexStageInfo.setModule(Shader[0]);
    vertexStageInfo.setPName("main");

    vk::PipelineShaderStageCreateInfo fragmentStageInfo{};
    fragmentStageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
    fragmentStageInfo.setModule(Shader[1]);
    fragmentStageInfo.setPName("main");

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = {vertexStageInfo, fragmentStageInfo};

    // Viewport state
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    // Formats
    vk::Format colorFormat = vk::Format::eR32G32B32A32Sfloat;

    vk::raii::Pipeline GraphicsPipeline = std::move(

        GraphicsPipelineFactory
        ->SetShaderStages(shaderStages)
        .SetVertexInput(vertexInputInfo)
        .SetInputAssembly(inputAssembly)
        .SetRasterizer(rasterizer)
        .SetMultisampling(multisampling)
        .SetColorBlendAttachments({colorBlendAttachment})
        .SetViewportState(viewportState)
        .SetDynamicStates({vk::DynamicState::eScissor, vk::DynamicState::eViewport})
        .SetDepthStencil(depthStencil)
        .SetLayout(pipelineLayout)
        .SetColorFormats({colorFormat})
        .SetDepthFormat({})
        .Build()

    );


    const glm::vec3 eye = glm::vec3(0.0f);

    glm::mat4 captureViews[6] = {
        glm::lookAt(eye, eye + glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)), // +X
        glm::lookAt(eye, eye + glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)), // -X
        glm::lookAt(eye, eye + glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)), // +Y
        glm::lookAt(eye, eye + glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)), // -Y
        glm::lookAt(eye, eye + glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)), // +Z
        glm::lookAt(eye, eye + glm::vec3(0, 0, -1), glm::vec3(0, -1, 0)), // -Z
    };

    glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    captureProj[1][1] *= -1.0f;


    auto cmd = m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool);

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    cmd.begin(beginInfo);


    ImageFactory::ShiftImageLayout(
    cmd,
    inImage,
    vk::ImageLayout::eReadOnlyOptimal,
    vk::AccessFlagBits::eNone,
    vk::AccessFlagBits::eColorAttachmentRead,
    vk::PipelineStageFlagBits::eNone,
    vk::PipelineStageFlagBits::eColorAttachmentOutput, inLayerCount
);

    ImageFactory::ShiftImageLayout(
    cmd,
    outImage,
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::AccessFlagBits::eNone,
    vk::AccessFlagBits::eColorAttachmentWrite,
    vk::PipelineStageFlagBits::eNone,
    vk::PipelineStageFlagBits::eColorAttachmentOutput, 6
);

    for (uint32_t idx = 0; idx < 6; idx++) {



        vk::RenderingAttachmentInfo RenderAttchInfo{};
        RenderAttchInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
        RenderAttchInfo.imageView = outImageViews[idx];
        RenderAttchInfo.loadOp = vk::AttachmentLoadOp::eClear;
        RenderAttchInfo.storeOp = vk::AttachmentStoreOp::eStore;
        RenderAttchInfo.clearValue = {};

        vk::RenderingInfo RenderingInfo{};
        RenderingInfo.colorAttachmentCount = 1;
        RenderingInfo.layerCount = 1;
        RenderingInfo.pColorAttachments = &RenderAttchInfo;
        RenderingInfo.pStencilAttachment = nullptr;
        RenderingInfo.renderArea = vk::Rect2D{VkOffset2D{}, renderArea};


        vk::DescriptorSet hppDs = ds;

        vk::Viewport viewport = {
            0, 0,
            static_cast<float>(RenderingInfo.renderArea.extent.width),
            static_cast<float>(RenderingInfo.renderArea.extent.height),
            0.0f, 1.0f
        };
        vk::Rect2D scissor = RenderingInfo.renderArea;


        cmd.beginRendering(RenderingInfo);
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, GraphicsPipeline);
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, hppDs, {});

        cmd.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0,
                          vk::ArrayProxy<const glm::mat4>{captureViews[idx]});
        cmd.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eVertex, sizeof(glm::mat4),
                          vk::ArrayProxy<const glm::mat4>{captureProj});
        cmd.setViewport(0, viewport);
        cmd.setScissor(0, scissor);

        cmd.draw(36, 1, 0, 0);
        cmd.endRendering();
    }

    cmd.end();

    m_DescriptorSetFactory->ResetFactory();


    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*cmd;

    vk::FenceCreateInfo fenceInfo{};

    auto fence = m_Device->createFence(fenceInfo);
    m_GraphicsQueue->submit({submitInfo}, fence);
    auto result = m_Device->waitForFences({fence}, true,UINT64_MAX);
    if (result != vk::Result::eSuccess) {
        std::cerr << "Failed to wait fences" << std::endl;
    }
}


void VulkanWindow::Run() {
    InitWindow();
    InitVulkan();
    MainLoop();
    Cleanup();
}

void VulkanWindow::InitWindow() {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_Window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferResizeCallback);
}


void VulkanWindow::CreateVmaAllocator() {
    VmaAllocatorCreateInfo vmaCreateInfo{};
    vmaCreateInfo.device = **m_Device;
    vmaCreateInfo.instance = **m_Instance;
    vmaCreateInfo.physicalDevice = **m_PhysicalDevice;
    vmaCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    vmaCreateInfo.pAllocationCallbacks = nullptr;
    vmaCreateAllocator(&vmaCreateInfo, &m_VmaAllocator);
}

void VulkanWindow::InitVulkan() {
    m_CurrentScreenSize = {WIDTH, HEIGHT};

    m_Buffer = std::make_unique<Buffer>();
    m_Camera = std::make_unique<Camera>();
    m_Camera->UpdateTarget();

    m_AllocationTracker = std::make_unique<ResourceTracker>();

    m_Instance = std::make_unique<vk::raii::Instance>(
        m_InstanceFactory->Build_Instance(m_Context, instanceExtensions, validationLayers));
    m_DebugMessenger = std::make_unique<vk::raii::DebugUtilsMessengerEXT>(
        m_DebugMessengerFactory->Build_DebugMessenger(*m_Instance));
    CreateSurface();
    m_PhysicalDevice = std::make_unique<vk::raii::PhysicalDevice>(
        PhysicalDevicePicker::ChoosePhysicalDevice(*m_Instance));
    m_Device = std::make_unique<vk::raii::Device>(m_LogicalDeviceFactory->Build_Device(*m_PhysicalDevice, m_Surface));

    CreateVmaAllocator();

    m_DescriptorSetFactory = std::make_unique<DescriptorSetFactory>(*m_Device);
    m_FrameDescriptorSetLayout = std::make_unique<vk::raii::DescriptorSetLayout>(
        std::move(
            m_DescriptorSetFactory
            ->AddBinding(0, vk::DescriptorType::eUniformBuffer,
                         vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .AddBinding(1, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(2, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(3, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(4, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(5, vk::DescriptorType::eUniformBuffer,
                        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .AddBinding(6, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment,
                        static_cast<uint32_t>(m_DirectionalLights.size()))
            .AddBinding(7, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(8, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment)
            .Build()
        )
    );

    m_DescriptorSetFactory->ResetFactory();

    m_UniformBufferInfo = m_Buffer->CreateMapped(m_VmaAllocator, sizeof(MVP),
                                                 vk::BufferUsageFlagBits::eUniformBuffer,
                                                 VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                 m_AllocationTracker.get(), "MVP");

    m_ShadowUBOBufferInfo = m_Buffer->CreateMapped(m_VmaAllocator, sizeof(ShadowMVP),
                                                   vk::BufferUsageFlagBits::eUniformBuffer,
                                                   VMA_MEMORY_USAGE_CPU_TO_GPU,
                                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                   m_AllocationTracker.get(), "ShadowMVP");

    m_PointLightBufferInfo = m_Buffer->CreateMapped(
        m_VmaAllocator,
        sizeof(PointLight) * m_PointLights.size(),
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        m_AllocationTracker.get(),
        "PointLights"
    );

    m_DirectionalLightBufferInfo = m_Buffer->CreateMapped(
        m_VmaAllocator,
        sizeof(DirectionalLight) * m_DirectionalLights.size(),
        vk::BufferUsageFlagBits::eStorageBuffer,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        m_AllocationTracker.get(),
        "DirectionalLights"
    );


    Buffer::UploadData(m_PointLightBufferInfo, m_PointLights.data(), sizeof(PointLight) * m_PointLights.size());
    Buffer::UploadData(m_DirectionalLightBufferInfo, m_DirectionalLights.data(),
                       sizeof(DirectionalLight) * m_DirectionalLights.size());


    vk::SamplerCreateInfo samplerInfo = {
        {},
        vk::Filter::eLinear,
        vk::Filter::eLinear,
        vk::SamplerMipmapMode::eLinear,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        vk::SamplerAddressMode::eRepeat,
        0.0f,
        VK_TRUE,
        16.0f,
        VK_FALSE,
        vk::CompareOp::eNever,
        0.0f,
        0.0f,
        vk::BorderColor::eIntOpaqueBlack,
        VK_FALSE
    };


    m_Sampler = std::make_unique<vk::raii::Sampler>(*m_Device, samplerInfo);


    // Create Swapchain and Depth Image
    m_SwapChain = std::make_unique<vk::raii::SwapchainKHR>(
        m_SwapChainFactory->Build_SwapChain(*m_Device, *m_PhysicalDevice, m_Surface, WIDTH, HEIGHT)
    );

    m_DepthImageFactory = std::make_unique<DepthImageFactory>(
        *m_Device,
        m_VmaAllocator,
        m_SwapChainFactory->Extent,
        m_VmaAllocatorsDeletionQueue
    );

    CreateSemaphoreAndFences();

    uint32_t QueueIdx = m_LogicalDeviceFactory->FindQueueFamilyIndex(*m_PhysicalDevice, m_Surface,
                                                                     vk::QueueFlagBits::eGraphics);
    VkQueue rawQueue = *m_Device->getQueue(QueueIdx, 0);
    m_GraphicsQueue = std::make_unique<vk::raii::Queue>(*m_Device, rawQueue);

    m_CmdPool = std::make_unique<vk::raii::CommandPool>(m_Renderer->CreateCommandPool(*m_Device, QueueIdx));

    auto swapImg = m_SwapChain->getImages();
    m_SwapChainImages.resize(swapImg.size());

    for (size_t i = 0; i < swapImg.size(); i++) {
        m_SwapChainImages[i].image = swapImg[i];
        m_SwapChainImages[i].imageAspectFlags = vk::ImageAspectFlagBits::eColor;
    }

    m_DepthPass = std::make_unique<DepthPass>(*m_Device, m_CommandBuffers);

    m_DepthPass->CreateImage(m_VmaAllocator, m_AllocationTracker.get(), m_DepthImageFactory->GetFormat(),
                             m_SwapChainFactory->Extent.width, m_SwapChainFactory->Extent.height);

    m_GBufferPass = std::make_unique<GBufferPass>(*m_Device, m_CommandBuffers);
    m_GBufferPass->CreateGBuffer(m_VmaAllocator, m_AllocationTracker.get(), m_SwapChainFactory->Extent.width,
                                 m_SwapChainFactory->Extent.height);

    m_ShadowPass = std::make_unique<ShadowPass>(*m_Device, m_CommandBuffers);
    m_ShadowPass->CreateShadowResources(static_cast<uint32_t>(m_DirectionalLights.size()), m_VmaAllocator,
                                        m_VmaAllocatorsDeletionQueue,
                                        m_AllocationTracker.get(), static_cast<uint32_t>(m_ShadowResolution.x),
                                        static_cast<uint32_t>(m_ShadowResolution.y));

    LoadMesh();

    m_GlobalDescriptorSetLayout = std::make_unique<vk::raii::DescriptorSetLayout>(
        std::move(
            m_DescriptorSetFactory
            ->AddBinding(0, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(1, vk::DescriptorType::eSampledImage, vk::ShaderStageFlagBits::eFragment,
                        static_cast<uint32_t>(m_ImageResource.size()))
            .AddBinding(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eFragment)
            .AddBinding(4, vk::DescriptorType::eSampler, vk::ShaderStageFlagBits::eFragment)

            .Build()
        )
    );


    m_DescriptorSets = std::make_unique<DescriptorSets>(*m_Device);
    m_DescriptorSets->CreateDescriptorPool(static_cast<uint32_t>(m_DirectionalLights.size()));

    CreateCommandBuffers();


    std::vector<vk::ShaderModule> CubemapSources;
    auto ShaderModules = ShaderFactory::Build_ShaderModules(*m_Device, "shaders/cubemapvert.spv",
                                                            "shaders/cubemapfrag.spv");
    for (auto &shader: ShaderModules) {
        vk::DebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.pObjectName = "cubemap shader";
        nameInfo.objectType = vk::ObjectType::eShaderModule;
        nameInfo.objectHandle = uint64_t(&**shader);

        m_Device->setDebugUtilsObjectNameEXT(nameInfo);


        CubemapSources.emplace_back(std::move(shader));
    }

    std::unique_ptr<Buffer> imgBuffer = std::make_unique<Buffer>();

    ImageResource hdrImage = ImageFactory::LoadHDRTexture(imgBuffer.get(),
                                                          "circus_arena_4k.hdr",
                                                          *m_Device,
                                                          m_VmaAllocator,
                                                          *m_CmdPool,
                                                          *m_GraphicsQueue,
                                                          vk::Format::eR32G32B32A32Sfloat,
                                                          vk::ImageAspectFlagBits::eColor,
                                                          m_AllocationTracker.get()
    );
    m_AllocationTracker->TrackAllocation(hdrImage.allocation, "hdr image");

    vk::ImageView hdrImageView = ImageFactory::CreateImageView(*m_Device, hdrImage.image, hdrImage.format,
                                                               hdrImage.imageAspectFlags, m_AllocationTracker.get(),
                                                               "hdr img view");


    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.extent = vk::Extent3D{(hdrImage.extent.width / 4), (hdrImage.extent.height / 2), 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6;
    imageInfo.format = vk::Format::eR32G32B32A32Sfloat;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;
    imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;

    ImageFactory::CreateImage(*m_Device, m_VmaAllocator, m_CubemapImage, imageInfo, "Cubemap image");
    m_CubemapImage.imageAspectFlags = vk::ImageAspectFlagBits::eColor;
    m_CubemapImage.imageLayout = vk::ImageLayout::eUndefined;

    m_AllocationTracker->TrackAllocation(m_CubemapImage.allocation, "cubemap image");


    // cubemap
    std::array<vk::ImageView, 6> imageviews{};
    for (size_t idx = 0; idx < 6; ++idx) {
        imageviews[idx] = ImageFactory::CreateImageView(*m_Device, m_CubemapImage.image, m_CubemapImage.format,
                                                        m_CubemapImage.imageAspectFlags, m_AllocationTracker.get(),
                                                        "img view " + std::to_string(idx), static_cast<uint32_t>(idx));
    }


    RenderToCubemap(CubemapSources, hdrImage, hdrImageView, *m_Sampler, m_CubemapImage, imageviews,
                    {imageInfo.extent.width, imageInfo.extent.height});
    m_CubemapImage.extent = vk::Extent2D{imageInfo.extent.width, imageInfo.extent.height};


    // irradiance

    std::vector<vk::ShaderModule> IrradianceCubemapSources;
    auto IrrShaderModules = ShaderFactory::Build_ShaderModules(*m_Device, "shaders/cubemapvert.spv",
                                                               "shaders/irrCubemapfrag.spv");
    for (auto &shader: IrrShaderModules) {
        vk::DebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.pObjectName = "cubemap shader";
        nameInfo.objectType = vk::ObjectType::eShaderModule;
        nameInfo.objectHandle = uint64_t(&**shader);

        m_Device->setDebugUtilsObjectNameEXT(nameInfo);


        IrradianceCubemapSources.emplace_back(std::move(shader));
    }

    uint32_t faceSize = m_CubemapImage.extent.width;
    uint32_t irrSize = std::max(1u, faceSize / 16u);

    vk::ImageCreateInfo irrimageInfo{};
    irrimageInfo.imageType = vk::ImageType::e2D;
    irrimageInfo.extent = vk::Extent3D{irrSize, irrSize, 1};
    irrimageInfo.mipLevels = 1;
    irrimageInfo.arrayLayers = 6;
    irrimageInfo.format = vk::Format::eR32G32B32A32Sfloat;
    irrimageInfo.tiling = vk::ImageTiling::eOptimal;
    irrimageInfo.initialLayout = vk::ImageLayout::eUndefined;
    irrimageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment |
                         vk::ImageUsageFlagBits::eSampled;
    irrimageInfo.samples = vk::SampleCountFlagBits::e1;
    irrimageInfo.sharingMode = vk::SharingMode::eExclusive;
    irrimageInfo.flags = vk::ImageCreateFlagBits::eCubeCompatible;


    ImageFactory::CreateImage(*m_Device, m_VmaAllocator, m_IrradianceImage, irrimageInfo, "IrradianceImage image");
    m_IrradianceImage.imageAspectFlags = vk::ImageAspectFlagBits::eColor;
    m_IrradianceImage.imageLayout = vk::ImageLayout::eUndefined;
    m_AllocationTracker->TrackAllocation(m_IrradianceImage.allocation, "irradiance image");

    std::array<vk::ImageView, 6> irrimageviews{};
    for (size_t idx = 0; idx < 6; ++idx) {
        irrimageviews[idx] = ImageFactory::CreateImageView(*m_Device, m_IrradianceImage.image, m_IrradianceImage.format,
                                                           m_IrradianceImage.imageAspectFlags,
                                                           m_AllocationTracker.get(),
                                                           "img view " + std::to_string(idx), static_cast<uint32_t>(idx));
    }

    m_CubemapImageView = ImageFactory::CreateImageView(*m_Device, m_CubemapImage.image, m_CubemapImage.format,
                                                       m_CubemapImage.imageAspectFlags, m_AllocationTracker.get(),
                                                       "CubemapImage", 0, vk::ImageViewType::eCube);


    RenderToCubemap(IrradianceCubemapSources, m_CubemapImage, m_CubemapImageView, *m_Sampler, m_IrradianceImage,
                    irrimageviews, {irrimageInfo.extent.width, irrimageInfo.extent.height}, 6);

    m_IrradianceImage.extent = vk::Extent2D{irrimageInfo.extent.width, irrimageInfo.extent.height};


    m_IrradianceImageView = ImageFactory::CreateImageView(*m_Device, m_IrradianceImage.image, m_IrradianceImage.format,
                                                          m_IrradianceImage.imageAspectFlags, m_AllocationTracker.get(),
                                                          "IrradianceImage", 0, vk::ImageViewType::eCube);


    m_AllocationTracker->UntrackImageView(hdrImageView);
    vkDestroyImageView(**m_Device, hdrImageView, nullptr);

    for (size_t idx = 0; idx < 6; ++idx) {
        m_AllocationTracker->UntrackImageView(imageviews[idx]);
        vkDestroyImageView(**m_Device, imageviews[idx], nullptr);
    }

    // Cleanup per-face views
    for (size_t idx = 0; idx < 6; ++idx) {
        m_AllocationTracker->UntrackImageView(irrimageviews[idx]);
        vkDestroyImageView(**m_Device, irrimageviews[idx], nullptr);
    }


    m_DescriptorSets->CreateGlobalDescriptorSet(
        **m_GlobalDescriptorSetLayout, *m_Sampler,
        std::make_pair(m_PointLightBufferInfo, static_cast<uint32_t>(m_PointLights.size())),
        std::make_pair(m_DirectionalLightBufferInfo, static_cast<uint32_t>(m_DirectionalLights.size())),
        m_ImageResource,
        m_SwapChainImageViews, m_ShadowPass->GetSampler()
    );

    m_DescriptorSets->CreateFrameDescriptorSet(**m_FrameDescriptorSetLayout, m_GBufferPass->GetImageViews(),
                                               m_DepthPass->GetImageView(), m_UniformBufferInfo, m_ShadowUBOBufferInfo,
                                               m_ShadowPass->GetImageView(), m_CubemapImageView, m_IrradianceImageView);

    m_GBufferPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);
    m_DepthPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);
    m_ShadowPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);

    CreatePipelineLayout();
    m_GBufferPass->m_PipelineLayout = **m_PipelineLayout;
    m_DepthPass->m_PipelineLayout = **m_PipelineLayout;
    m_ShadowPass->m_PipelineLayout = **m_PipelineLayout;

    m_GBufferPass->SetMeshes(m_Meshes);
    m_DepthPass->SetMeshes(m_Meshes);
    m_ShadowPass->SetMeshes(m_Meshes);

    std::pair Format = {m_SwapChainFactory->Format.format, m_DepthImageFactory->GetFormat()};

    m_DepthPass->CreatePipeline(static_cast<uint32_t>(m_ImageResource.size()), Format);
    m_GBufferPass->CreatePipeline(static_cast<uint32_t>(m_ImageResource.size()), m_DepthImageFactory->GetFormat());
    m_ShadowPass->CreatePipeline(static_cast<uint32_t>(m_ImageResource.size()), m_DepthImageFactory->GetFormat());

    // create color pass
    std::pair lights = {m_DirectionalLights, m_PointLights};
    m_ColorPass = std::make_unique<ColorPass>(*m_Device, **m_PipelineLayout, m_CommandBuffers, lights, Format);
    m_ColorPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);
    m_ShadowPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);


    BeginCommandBuffer();
    PrepareFrame();

    for (const auto &[idx, light]: std::ranges::views::enumerate(m_DirectionalLights)) {
        UpdateShadowUBO(static_cast<uint32_t>(idx));


        m_ShadowPass->DoPass(static_cast<uint32_t>(idx), m_CurrentFrame, static_cast<uint32_t>(m_ShadowResolution.x),
                             static_cast<uint32_t>(m_ShadowResolution.y));


        ImageFactory::ShiftImageLayout(
            *m_CommandBuffers[m_CurrentFrame],
            m_ShadowPass->GetImage()[idx],
            vk::ImageLayout::eDepthReadOnlyOptimal,
            vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            vk::AccessFlagBits::eShaderRead,
            vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::PipelineStageFlagBits::eFragmentShader
        );
    }
    EndCommandBuffer();
    SubmitOffscreen();


    m_AllocationTracker->UntrackAllocation(hdrImage.allocation);
    vmaDestroyImage(m_VmaAllocator, hdrImage.image, hdrImage.allocation);

    m_VmaAllocatorsDeletionQueue.emplace_back([&](VmaAllocator) {
        Buffer::Destroy(m_VmaAllocator, m_UniformBufferInfo.m_Buffer, m_UniformBufferInfo.m_Allocation,
                        m_AllocationTracker.get());
        Buffer::Destroy(m_VmaAllocator, m_PointLightBufferInfo.m_Buffer, m_PointLightBufferInfo.m_Allocation,
                        m_AllocationTracker.get());
        Buffer::Destroy(m_VmaAllocator, m_DirectionalLightBufferInfo.m_Buffer,
                        m_DirectionalLightBufferInfo.m_Allocation, m_AllocationTracker.get());
        Buffer::Destroy(m_VmaAllocator, m_ShadowUBOBufferInfo.m_Buffer, m_ShadowUBOBufferInfo.m_Allocation,
                        m_AllocationTracker.get());


        m_AllocationTracker->UntrackImageView(m_CubemapImageView);
        vkDestroyImageView(**m_Device, m_CubemapImageView, nullptr);

        m_AllocationTracker->UntrackAllocation(m_CubemapImage.allocation);
        vmaDestroyImage(m_VmaAllocator, m_CubemapImage.image, m_CubemapImage.allocation);

        m_AllocationTracker->UntrackImageView(m_IrradianceImageView);
        vkDestroyImageView(**m_Device, m_IrradianceImageView, nullptr);

        m_AllocationTracker->UntrackAllocation(m_IrradianceImage.allocation);
        vmaDestroyImage(m_VmaAllocator, m_IrradianceImage.image, m_IrradianceImage.allocation);
    });

    m_AllocationTracker->PrintAllocations();
}

void VulkanWindow::DrawFrame() {
    UpdateUBO();
    int width, height;
    glfwGetFramebufferSize(m_Window, &width, &height);
    m_CurrentScreenSize = glm::vec2(width, height);

    HandleFramebufferResize(width, height);

    PrepareFrame();
    uint32_t imageIndex = AcquireSwapchainImage();

    BeginCommandBuffer();

    TransitionInitialLayouts(imageIndex);

    ImageFactory::ShiftImageLayout(
        *m_CommandBuffers[m_CurrentFrame],
        m_DepthPass->GetImage(),
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::AccessFlagBits::eNone,
        vk::AccessFlagBits::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::PipelineStageFlagBits::eEarlyFragmentTests
    );

    m_DepthPass->DoPass(m_CurrentFrame, width, height);

    m_GBufferPass->DoPass(m_DepthPass->GetImageView(), m_CurrentFrame, width, height);
    m_GBufferPass->PrepareImagesForRead(m_CurrentFrame);

    ImageFactory::ShiftImageLayout(*m_CommandBuffers[m_CurrentFrame],
                                   m_DepthPass->GetImage(),
                                   vk::ImageLayout::eReadOnlyOptimal,
                                   vk::AccessFlagBits::eNone,
                                   vk::AccessFlagBits::eColorAttachmentRead,
                                   vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput);


    ImageFactory::ShiftImageLayout(*m_CommandBuffers[m_CurrentFrame],
                                   m_CubemapImage,
                                   vk::ImageLayout::eReadOnlyOptimal,
                                   vk::AccessFlagBits::eNone,
                                   vk::AccessFlagBits::eColorAttachmentRead,
                                   vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput, 6);

    ImageFactory::ShiftImageLayout(*m_CommandBuffers[m_CurrentFrame],
                                   m_IrradianceImage,
                                   vk::ImageLayout::eReadOnlyOptimal,
                                   vk::AccessFlagBits::eNone,
                                   vk::AccessFlagBits::eColorAttachmentRead,
                                   vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput, 6);

    m_ColorPass->DoPass(m_SwapChainFactory->m_ImageViews, m_CurrentFrame, imageIndex, width, height);

    TransitionForPresentation(imageIndex);

    EndCommandBuffer();

    SubmitFrame();

    PresentFrame(imageIndex);

    m_CurrentFrame = (m_CurrentFrame + 1) % m_FramesInFlight;
}

void VulkanWindow::HandleFramebufferResize(int width, int height) {
    if (!m_bFrameBufferResized) return;

    m_Device->waitIdle();

    m_SwapChain.reset();
    m_SwapChainFactory->m_ImageViews.clear();

    m_SwapChain = std::make_unique<vk::raii::SwapchainKHR>(
        m_SwapChainFactory->Build_SwapChain(*m_Device, *m_PhysicalDevice, m_Surface, width, height));

    m_SwapChainImages.clear();
    auto swapImg = m_SwapChain->getImages();
    m_SwapChainImages.resize(swapImg.size());

    for (size_t i = 0; i < swapImg.size(); i++) {
        m_SwapChainImages[i].image = swapImg[i];
        m_SwapChainImages[i].imageAspectFlags = vk::ImageAspectFlagBits::eColor;
    }

    m_DepthPass->RecreateImage(m_VmaAllocator, m_AllocationTracker.get(), std::get<1>(m_DepthPass->GetFormat()),
                               width, height);
    m_GBufferPass->RecreateGBuffer(m_VmaAllocator, m_AllocationTracker.get(), m_SwapChainFactory->Extent.width,
                                   m_SwapChainFactory->Extent.height);

    auto transitionCmd = m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool);
    transitionCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    m_GBufferPass->WindowResizeShiftLayout(transitionCmd);
    m_DepthPass->WindowResizeShiftLayout(transitionCmd);
    transitionCmd.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*transitionCmd;

    // Submit and wait for completion
    m_GraphicsQueue->submit(submitInfo, VK_NULL_HANDLE);
    m_GraphicsQueue->waitIdle();


    m_DescriptorSets->CreateDescriptorPool(static_cast<uint32_t>(m_DirectionalLights.size()));
    m_DescriptorSets->CreateGlobalDescriptorSet(
        **m_GlobalDescriptorSetLayout, *m_Sampler,
        std::make_pair(m_PointLightBufferInfo, static_cast<uint32_t>(m_PointLights.size())),
        std::make_pair(m_DirectionalLightBufferInfo, static_cast<uint32_t>(m_DirectionalLights.size())),
        m_ImageResource,
        m_SwapChainImageViews, m_ShadowPass->GetSampler()
    );
    m_DescriptorSets->CreateFrameDescriptorSet(*m_FrameDescriptorSetLayout, m_GBufferPass->GetImageViews(),
                                               m_DepthPass->GetImageView(), m_UniformBufferInfo, m_ShadowUBOBufferInfo,
                                               m_ShadowPass->GetImageView(), m_CubemapImageView, m_IrradianceImageView);

    m_GBufferPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);
    m_DepthPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);
    m_ColorPass->m_DescriptorSets = m_DescriptorSets->GetDescriptorSets(m_CurrentFrame);

    m_bFrameBufferResized = false;
}

void VulkanWindow::PrepareFrame() {
    auto result = m_Device->waitForFences({*m_RenderFinishedFence}, VK_TRUE, UINT64_MAX);
    if (result != vk::Result::eSuccess) {
        std::cerr << "Failed to wait fences" << std::endl;
    }

    m_Device->resetFences({*m_RenderFinishedFence});
}

uint32_t VulkanWindow::AcquireSwapchainImage() const {
    vk::AcquireNextImageInfoKHR acquireInfo{};
    acquireInfo.swapchain = **m_SwapChain;
    acquireInfo.timeout = 1'000'000'000ULL;
    acquireInfo.semaphore = *m_ImageAvailableSemaphore;
    acquireInfo.deviceMask = 1;

    auto result = m_Device->acquireNextImage2KHR(acquireInfo);
    if (result.first != vk::Result::eSuccess && result.first != vk::Result::eSuboptimalKHR)
        throw std::runtime_error("Failed to acquire swap chain image!");


    return result.second;
}


void VulkanWindow::BeginCommandBuffer() const {
    m_CommandBuffers[m_CurrentFrame]->reset();
    m_CommandBuffers[m_CurrentFrame]->begin(vk::CommandBufferBeginInfo());
}

void VulkanWindow::TransitionInitialLayouts(uint32_t imageIndex) {
    ImageFactory::ShiftImageLayout(*m_CommandBuffers[m_CurrentFrame],
                                   m_SwapChainImages[imageIndex], vk::ImageLayout::eColorAttachmentOptimal,
                                   vk::AccessFlagBits::eNone, vk::AccessFlagBits::eColorAttachmentWrite,
                                   vk::PipelineStageFlagBits::eTopOfPipe,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput);
}


void VulkanWindow::TransitionForPresentation(uint32_t imageIndex) {
    ImageFactory::ShiftImageLayout(*m_CommandBuffers[m_CurrentFrame],
                                   m_SwapChainImages[imageIndex], vk::ImageLayout::ePresentSrcKHR,
                                   vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eNone,
                                   vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                   vk::PipelineStageFlagBits::eBottomOfPipe);
}

void VulkanWindow::EndCommandBuffer() const {
    m_CommandBuffers[m_CurrentFrame]->end();
}

void VulkanWindow::SubmitFrame() const {
    vk::SubmitInfo submitInfo{};
    vk::Semaphore waitSemaphores[] = {*m_ImageAvailableSemaphore};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::Semaphore signalSemaphores[] = {*m_RenderFinishedSemaphore};

    vk::CommandBuffer cmd = *m_CommandBuffers[m_CurrentFrame];
    submitInfo.setWaitSemaphores(waitSemaphores);
    submitInfo.setWaitDstStageMask(waitStages);
    submitInfo.setCommandBuffers(cmd);
    submitInfo.setSignalSemaphores(signalSemaphores);

    m_GraphicsQueue->submit(submitInfo, **m_RenderFinishedFence);
}


void VulkanWindow::SubmitOffscreen() const {
    vk::SubmitInfo submitInfo{};
    vk::Semaphore waitSemaphores[] = {*m_ImageAvailableSemaphore};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::Semaphore signalSemaphores[] = {*m_RenderFinishedSemaphore};

    vk::CommandBuffer cmd = *m_CommandBuffers[m_CurrentFrame];
    submitInfo.setWaitDstStageMask(waitStages);
    submitInfo.setCommandBuffers(cmd);
    submitInfo.setSignalSemaphoreCount(0);
    submitInfo.setWaitSemaphoreCount(0);

    m_GraphicsQueue->submit(submitInfo, **m_RenderFinishedFence);
}

void VulkanWindow::PresentFrame(uint32_t imageIndex) const {
    vk::PresentInfoKHR presentInfo{};
    presentInfo.setWaitSemaphores(**m_RenderFinishedSemaphore);
    presentInfo.setSwapchains(**m_SwapChain);
    presentInfo.setImageIndices(imageIndex);
    auto result = m_GraphicsQueue->presentKHR(presentInfo);
    if (result != vk::Result::eSuccess) {
        std::cerr << "Failed to present" << std::endl;
    }

}

void VulkanWindow::CreateSemaphoreAndFences() {
    m_ImageAvailableSemaphore = std::make_unique<vk::raii::Semaphore>(*m_Device, vk::SemaphoreCreateInfo());
    m_RenderFinishedSemaphore = std::make_unique<vk::raii::Semaphore>(*m_Device, vk::SemaphoreCreateInfo());
    m_RenderFinishedFence = std::make_unique<vk::raii::Fence>(
        *m_Device, vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
}

void VulkanWindow::LoadMesh() {
    m_MeshFactory = std::make_unique<MeshFactory>();
    m_MeshCmdBuffer = std::make_unique<vk::raii::CommandBuffer>(
        std::move(m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool)));

    m_Meshes = m_MeshFactory->LoadModelFromGLTF("models/sponza/Sponza.gltf",
                                                m_VmaAllocator, m_VmaAllocatorsDeletionQueue,
                                                **m_MeshCmdBuffer, *m_GraphicsQueue, *m_Device,
                                                *m_CmdPool, m_ImageResource,
                                                m_SwapChainImageViews, m_AllocationTracker.get());
}

void VulkanWindow::CreatePipelineLayout() {
    std::vector<vk::DescriptorSetLayout> DescriptorSetLayouts{
        *m_FrameDescriptorSetLayout, *m_GlobalDescriptorSetLayout
    };

    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(Material);
    pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::PipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.setLayoutCount = static_cast<uint32_t>(DescriptorSetLayouts.size());
    layoutInfo.pSetLayouts = DescriptorSetLayouts.data();
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    m_PipelineLayout = std::make_unique<vk::raii::PipelineLayout>(*m_Device, layoutInfo);
}

void VulkanWindow::CreateCommandBuffers() {
    for (size_t frames{}; frames < m_FramesInFlight; ++frames) {
        m_CommandBuffers.emplace_back(
            std::make_unique<vk::raii::CommandBuffer>(m_Renderer->CreateCommandBuffer(*m_Device, *m_CmdPool)));
    }
}
