#include <iostream>
#include "Window.h"

int main()
{

	glfwInit();

	vk::raii::Context Context{};

	VulkanWindow Window{Context};

	try
	{
		Window.Run();
	}
	catch (const std::exception& e)
	{

		std::cerr << e.what() << std::endl;
		glfwTerminate();

		return EXIT_FAILURE;
	}
		glfwTerminate();

}