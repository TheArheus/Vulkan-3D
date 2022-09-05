#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <windows.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include <spirv-headers/spirv.h>
#include <Volk/volk.h>
#include <Volk/volk.c>

// NOTE: remove this if possible
#include "meshoptimizer/meshoptimizer.h"
#include "meshoptimizer/allocator.cpp"
#include "meshoptimizer/clusterizer.cpp"
#include "meshoptimizer/indexcodec.cpp"
#include "meshoptimizer/indexgenerator.cpp"
#include "meshoptimizer/overdrawanalyzer.cpp"
#include "meshoptimizer/overdrawoptimizer.cpp"
#include "meshoptimizer/simplifier.cpp"
#include "meshoptimizer/spatialorder.cpp"
#include "meshoptimizer/vcacheanalyzer.cpp"
#include "meshoptimizer/vcacheoptimizer.cpp"
#include "meshoptimizer/vertexcodec.cpp"
#include "meshoptimizer/vertexfilter.cpp"
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

#include "mesh_loader.h"

global_variable bool IsRunning;
global_variable bool IsRtxSupported;
global_variable bool IsRtxEnabled;

struct shader_layout 
{
	VkPipelineLayout PipelineLayout;
	VkDescriptorSetLayout DescriptorSetLayout;
};

#include "shader.h"
#include "shader.cpp"

struct swapchain
{
	VkSwapchainKHR Handle;

	std::vector<VkImage> Images;
	std::vector<VkImageView> ImageViews;
	std::vector<VkFramebuffer> Framebuffers;

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

struct vertex
{
	uint16_t vx, vy, vz;
	u32 norm;
	uint16_t tu, tv;
};

// NOTE: Unfortunatly I am using 16 bit data instead of 8 bit data 
// as hlsl is not support 8 bit data;
struct meshlet
{
	u32 Vertices[64];
	u32 Indices[126*3]; // NOTE: up to 126 triangles
	u32 TriangleCount;
	u32 VertexCount;
};

struct mesh
{
	std::vector<vertex> Vertices;
	std::vector<u32> Indices;
	std::vector<meshlet> Meshlets;
};

