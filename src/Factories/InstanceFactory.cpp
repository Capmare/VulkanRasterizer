#include "InstanceFactory.h"
#include "GLFW/glfw3.h"

vk::raii::Instance InstanceFactory::Build_Instance(const vk::raii::Context &Context, std::vector<const char *> InstanceExtension, std::vector<const char *>
                                                   ValidationLayer)
{

	vk::ApplicationInfo ApplicationInfo{};
	ApplicationInfo.pApplicationName = "College";
	ApplicationInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
	ApplicationInfo.pEngineName = "VulkanEngine";
	ApplicationInfo.apiVersion = VK_API_VERSION_1_3;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	InstanceExtension.insert(
	InstanceExtension.end(),
	glfwExtensions,
	glfwExtensions + glfwExtensionCount
);

	vk::InstanceCreateInfo InstanceCreateInfo{};
	InstanceCreateInfo.setPApplicationInfo(&ApplicationInfo);
	InstanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(InstanceExtension.size());
	InstanceCreateInfo.ppEnabledExtensionNames = InstanceExtension.data();
	InstanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayer.size());
	InstanceCreateInfo.ppEnabledLayerNames = ValidationLayer.data();

	vk::raii::Instance Instance(Context, InstanceCreateInfo);

	return Instance;
}



