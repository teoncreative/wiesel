//
// Created by Metehan Gezer on 20.03.2023.
//

#ifndef WIESEL_UTILS_H
#define WIESEL_UTILS_H

#include <string>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>

namespace Wiesel {
	namespace Tools {
		std::string errorString(VkResult errorCode);
	}

	namespace Struct {
		struct Vertex {
			glm::vec2 pos;
			glm::vec3 color;
		};
	}
}

#define WIESEL_CHECK_VKRESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		std::cout << "Fatal : VkResult is \"" << Wiesel::Tools::errorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << "\n"; \
		assert(res == VK_SUCCESS);																		\
	}																									\
}
#endif //WIESEL_UTILS_H
