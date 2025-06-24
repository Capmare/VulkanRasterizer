//
// Created by capma on 6/19/2025.
//
#include <iostream>

#include "DebugMessengerFactory.h"


vk::raii::DebugUtilsMessengerEXT DebugMessengerFactory::Build_DebugMessenger(const vk::raii::Instance &instance) {
    vk::DebugUtilsMessengerCreateInfoEXT createInfo = populateDebugMessengerCreateInfo();

    return vk::raii::DebugUtilsMessengerEXT(instance, createInfo);

}

vk::DebugUtilsMessengerCreateInfoEXT DebugMessengerFactory::populateDebugMessengerCreateInfo() {
    return vk::DebugUtilsMessengerCreateInfoEXT{
	            {},
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
                vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
                &DebugMessengerFactory::debugCallback,
                nullptr
            };
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerFactory::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT ,
    VkDebugUtilsMessageTypeFlagsEXT ,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* )
{
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;

    // Return VK_TRUE if you want to break the call (for debugging)
    return VK_FALSE;
}
