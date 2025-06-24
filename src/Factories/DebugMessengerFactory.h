#ifndef DEBUGMESSENGER_H
#define DEBUGMESSENGER_H

#include "vulkan/vulkan.hpp"
#include "vulkan/vulkan_raii.hpp"

class DebugMessengerFactory {

public:
    DebugMessengerFactory() = default;
    virtual ~DebugMessengerFactory() = default;

    DebugMessengerFactory(const DebugMessengerFactory&) = delete;
    DebugMessengerFactory(DebugMessengerFactory&&) noexcept = delete;
    DebugMessengerFactory& operator=(const DebugMessengerFactory&) = delete;
    DebugMessengerFactory& operator=(DebugMessengerFactory&&) noexcept = delete;


    vk::raii::DebugUtilsMessengerEXT Build_DebugMessenger(const vk::raii::Instance &instance);

    static VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                           VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                           const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);

private:
    vk::DebugUtilsMessengerCreateInfoEXT populateDebugMessengerCreateInfo();


};



#endif //DEBUGMESSENGER_H
