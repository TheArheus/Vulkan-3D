#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <windows.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <spirv-headers/spirv.h>
#include <Volk/volk.h>
#include <Volk/volk.c>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/quaternion_transform.hpp>

// NOTE: remove this if possible
#include "meshoptimizer/meshoptimizer.h"
#include "meshoptimizer/allocator.cpp"
#include "meshoptimizer/clusterizer.cpp"
#include "meshoptimizer/indexcodec.cpp"
#include "meshoptimizer/indexgenerator.cpp"
#include "meshoptimizer/overdrawanalyzer.cpp"
#include "meshoptimizer/overdrawoptimizer.cpp"
#include "meshoptimizer/simplifier.cpp"
#include "meshoptimizer/vcacheanalyzer.cpp"
#include "meshoptimizer/vcacheoptimizer.cpp"
#include "meshoptimizer/vertexcodec.cpp"
#include "meshoptimizer/vfetchanalyzer.cpp"
#include "meshoptimizer/vfetchoptimizer.cpp"

#include "objparser.h"
#include "objparser.cpp"

typedef char					s8;
typedef short					s16;
typedef int						s32;
typedef long long int			s64;

typedef unsigned char			u8;
typedef unsigned short			u16;
typedef unsigned int			u32;
typedef unsigned long long int	u64;

typedef s32						bool32;
typedef s64						bool64;

#define internal static
#define global_variable static
#define local_persist static

#define ArraySize(Arr) (sizeof(Arr) / sizeof(Arr[0]))

#define VK_CHECK(call) \
	do { \
		VkResult CallResult = call; \
		assert(CallResult == VK_SUCCESS); \
	} while(0);

global_variable bool IsRunning;
global_variable bool IsRtxSupported;
global_variable bool IsRtxEnabled;
global_variable bool IsLodEnabled = true;
global_variable bool IsCullEnabled = true;

struct shader_layout 
{
	VkPipelineLayout PipelineLayout;
	VkDescriptorSetLayout DescriptorSetLayout;
};

#include "shader.h"
#include "shader.cpp"
#include "mesh_loader.h"

struct swapchain
{
	VkSwapchainKHR Handle;
	std::vector<VkImage> Images;
	u32 Width, Height;
};

#include "swapchain.cpp"

struct buffer
{
	VkBuffer Handle;
	VkDeviceMemory Memory;
	void* Data;
	size_t Size;
};

struct image
{
	VkImage Handle;
	VkImageView View;
	VkDeviceMemory Memory;
};

internal void DispatchMessages();

