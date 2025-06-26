//
// Created by capma on 6/26/2025.
//

#include "ShaderFactory.h"

#include "File.h"

std::vector<vk::raii::ShaderEXT> ShaderFactory::Build_Shader(const vk::raii::Device& device, const char *VertexFile,
                                                             const char *FragmentFile) {

    const char* EntryPoint = "main";

    auto vertex = File::ReadSpirvFile(VertexFile);
    if (vertex.empty()) throw std::runtime_error("Failed to read vertex shader");

    auto fragment = File::ReadSpirvFile(FragmentFile);
    if (fragment.empty()) throw std::runtime_error("Failed to read fragment shader");

    vk::ShaderCreateInfoEXT VertexInfo{};
    VertexInfo.setStage(vk::ShaderStageFlagBits::eVertex);
    VertexInfo.setNextStage(vk::ShaderStageFlagBits::eFragment);
    VertexInfo.setCodeType(vk::ShaderCodeTypeEXT::eSpirv);
    VertexInfo.setPCode(vertex.data());
    VertexInfo.setCodeSize(vertex.size() * sizeof(uint32_t));
    VertexInfo.setPName(EntryPoint);

    vk::ShaderCreateInfoEXT FragmentInfo{};
    FragmentInfo.setStage(vk::ShaderStageFlagBits::eFragment);
    FragmentInfo.setCodeType(vk::ShaderCodeTypeEXT::eSpirv);
    FragmentInfo.setPCode(fragment.data());
    FragmentInfo.setCodeSize(fragment.size() * sizeof(uint32_t));
    FragmentInfo.setPName(EntryPoint);

    std::vector<vk::ShaderCreateInfoEXT> infos = { VertexInfo, FragmentInfo };

    return device.createShadersEXT(infos);

}
