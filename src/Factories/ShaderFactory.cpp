//
// Created by capma on 6/26/2025.
//

#include "ShaderFactory.h"

#include "File.h"

/*std::vector<vk::raii::ShaderEXT> ShaderFactory::Build_Shader(const vk::raii::Device& device, const char *VertexFile,
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

}*/


std::vector<vk::raii::ShaderModule> ShaderFactory::Build_ShaderModules(const vk::raii::Device& device, const char* VertexFile, const char* FragmentFile) {
    auto vertexCode = File::ReadSpirvFile(VertexFile);
    if (vertexCode.empty()) throw std::runtime_error("Failed to read vertex shader");

    auto fragmentCode = File::ReadSpirvFile(FragmentFile);
    if (fragmentCode.empty()) throw std::runtime_error("Failed to read fragment shader");

    vk::ShaderModuleCreateInfo vertexModuleInfo{};
    vertexModuleInfo.setCode(vertexCode);

    vk::ShaderModuleCreateInfo fragmentModuleInfo{};
    fragmentModuleInfo.setCode(fragmentCode);

    std::vector<vk::raii::ShaderModule> modules;
    modules.reserve(2);  

    modules.emplace_back(device, vertexModuleInfo);
    modules.emplace_back(device, fragmentModuleInfo);

    return modules;
}