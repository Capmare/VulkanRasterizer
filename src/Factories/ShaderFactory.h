//
// Created by capma on 6/26/2025.
//

#ifndef SHADERFACTORY_H
#define SHADERFACTORY_H
#include <vector>

#include <vulkan/vulkan.hpp>
#include "vulkan/vulkan_raii.hpp"


class ShaderFactory {
public:
    ShaderFactory() = default;
    virtual ~ShaderFactory() = default;

    ShaderFactory(const ShaderFactory&) = delete;
    ShaderFactory(ShaderFactory&&) noexcept = delete;
    ShaderFactory& operator=(const ShaderFactory&) = delete;
    ShaderFactory& operator=(ShaderFactory&&) noexcept = delete;

    //static std::vector<vk::raii::ShaderEXT> Build_Shader(const vk::raii::Device &device, const char *VertexFile, const char *FragmentFile);

    static std::vector<vk::raii::ShaderModule> Build_ShaderModules(const vk::raii::Device &device, const char *VertexFile,
                                                            const char *FragmentFile);
};



#endif //SHADERFACTORY_H
