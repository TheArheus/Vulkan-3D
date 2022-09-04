#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <windows.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
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

struct swapchain
{
	VkSwapchainKHR Handle;

	std::vector<VkImage> Images;
	std::vector<VkImageView> ImageViews;
	std::vector<VkFramebuffer> Framebuffers;

	u32 Width, Height;
};

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

internal bool
LoadMesh(mesh& Result, const char* Path)
{
	ObjFile File;
	if(!objParseFile(File, Path))
	{
		return false;
	}
	size_t IndexCount = File.f_size / 3;
	std::vector<vertex> Vertices(IndexCount);

	for(u32 VertexIndex = 0;
		VertexIndex < IndexCount;
		++VertexIndex)
	{
		vertex& V = Vertices[VertexIndex];

		int VIndex = File.f[VertexIndex * 3 + 0];
		int VTextureIndex = File.f[VertexIndex * 3 + 1];
		int VNormalIndex = File.f[VertexIndex * 3 + 2];

		V.vx = meshopt_quantizeHalf(File.v[VIndex * 3 + 0]);
		V.vy = meshopt_quantizeHalf(File.v[VIndex * 3 + 1]);
		V.vz = meshopt_quantizeHalf(File.v[VIndex * 3 + 2]);

		float nx = VNormalIndex < 0 ? 0.f : File.vn[VNormalIndex * 3 + 0];
		float ny = VNormalIndex < 0 ? 0.f : File.vn[VNormalIndex * 3 + 1];
		float nz = VNormalIndex < 0 ? 0.f : File.vn[VNormalIndex * 3 + 2];

		V.norm = ((u8)(nx * 127 + 127) << 24) | ((u8)(ny * 127 + 127) << 16) | ((u8)(nz * 127 + 127) << 8) | 0;

		V.tu = meshopt_quantizeHalf(VTextureIndex < 0 ? 0.f : File.vt[VTextureIndex * 3 + 0]);
		V.tv = meshopt_quantizeHalf(VTextureIndex < 0 ? 0.f : File.vt[VTextureIndex * 3 + 1]);
	}

	std::vector<u32> Remap(IndexCount);
	size_t VertexCount = meshopt_generateVertexRemap(Remap.data(), 0, IndexCount, Vertices.data(), IndexCount, sizeof(vertex));

	Result.Vertices.resize(VertexCount);
	Result.Indices.resize(IndexCount);

	meshopt_remapVertexBuffer(Result.Vertices.data(), Vertices.data(), IndexCount, sizeof(vertex), Remap.data());
	meshopt_remapIndexBuffer(Result.Indices.data(), 0, IndexCount, Remap.data());

	meshopt_optimizeVertexCache(Result.Indices.data(), Result.Indices.data(), IndexCount, VertexCount);
	meshopt_optimizeVertexFetch(Result.Vertices.data(), Result.Indices.data(), IndexCount, Result.Vertices.data(), VertexCount, sizeof(vertex));

	return true;
}

internal void
BuildMeshlets(mesh& Mesh)
{
	meshlet Meshlet = {};
	std::vector<u32> MeshletVertices(Mesh.Vertices.size(), 0xffffffff);

	for(size_t TriangleIndex = 0;
		TriangleIndex < Mesh.Indices.size(); 
		TriangleIndex += 3)
	{
		u32 a = Mesh.Indices[TriangleIndex + 0];
		u32 b = Mesh.Indices[TriangleIndex + 1];
		u32 c = Mesh.Indices[TriangleIndex + 2];

		// NOTE: This is not optimal as it is using a lot of space
		// try to use atleast 16 bit data
		u32& VertexA = MeshletVertices[a];
		u32& VertexB = MeshletVertices[b];
		u32& VertexC = MeshletVertices[c];

		if((Meshlet.VertexCount + (VertexA == -1) + (VertexB == -1) + (VertexC == -1) > 64) || (Meshlet.TriangleCount >= 126))
		{
			Mesh.Meshlets.push_back(Meshlet);
			for (size_t InnerIndex = 0; InnerIndex < Meshlet.VertexCount; InnerIndex++) 
			{
				MeshletVertices[Meshlet.Vertices[InnerIndex]] = 0xffffffff;
			}
			Meshlet = {};
			
		}

		if(VertexA == -1)
		{
			VertexA = Meshlet.VertexCount;
			Meshlet.Vertices[Meshlet.VertexCount++] = a;
		}

		if(VertexB == -1)
		{
			VertexB = Meshlet.VertexCount;
			Meshlet.Vertices[Meshlet.VertexCount++] = b;
		}

		if(VertexC == -1)
		{
			VertexC = Meshlet.VertexCount;
			Meshlet.Vertices[Meshlet.VertexCount++] = c;
		}

		Meshlet.Indices[Meshlet.TriangleCount*3+0] = VertexA;
		Meshlet.Indices[Meshlet.TriangleCount*3+1] = VertexB;
		Meshlet.Indices[Meshlet.TriangleCount*3+2] = VertexC;
		Meshlet.TriangleCount++;
	}

	if(Meshlet.TriangleCount)
	{
		Mesh.Meshlets.push_back(Meshlet);
	}
}

internal void
DispatchMessages()
{
	MSG Message = {};
	while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch(Message.message)
		{
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYDOWN:
			case WM_KEYUP:
			{
				u32 KeyCode = (u32)Message.wParam;

				bool IsDown = ((Message.lParam & (1 << 31)) == 0);
				bool WasDown = ((Message.lParam & (1 << 30)) != 0);

				if(IsDown)
				{
					if(KeyCode == 'R')
					{
						IsRtxEnabled = !IsRtxEnabled;
					}
				}
			} break;

			default:
			{
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			} break;
		}
	}
}

VkBool32 DebugReportCallback(VkDebugReportFlagsEXT Flags, VkDebugReportObjectTypeEXT ObjectType, u64 Object, size_t Location, s32 MessageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	const char* ErrorType = (Flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ? "Error" :
							(Flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) ? "Warning" : 
							(Flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) ? "Performance Warning" :
							(Flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) ? "Debug" : "Unknown";

	char Message[4096];
	snprintf(Message, 4096, "[%s]: %s\n", ErrorType, pMessage);
	printf("%s", Message);

	return VK_FALSE;
}

internal VkInstance
CreateInstance(const char* ClassName)
{
	VkApplicationInfo AppInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
	AppInfo.apiVersion = VK_API_VERSION_1_2;
	AppInfo.pApplicationName = ClassName;

	std::vector<const char*> Layers = 
	{
#if _DEBUG
		"VK_LAYER_KHRONOS_validation"
#endif
	};

	std::vector<const char*> Extensions = 
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	};

	VkInstanceCreateInfo InstanceCreateInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	InstanceCreateInfo.pApplicationInfo = &AppInfo;
	InstanceCreateInfo.enabledLayerCount = (u32)Layers.size();
	InstanceCreateInfo.ppEnabledLayerNames = Layers.data();
	InstanceCreateInfo.enabledExtensionCount = (u32)Extensions.size();
	InstanceCreateInfo.ppEnabledExtensionNames = Extensions.data();

	VkInstance Instance = 0;
	VK_CHECK(vkCreateInstance(&InstanceCreateInfo, 0, &Instance));
	volkLoadInstance(Instance);

	return Instance;
}

internal u32
GetGraphicsQueueFamilyIndex(VkPhysicalDevice PhysicalDevice)
{
	u32 Result = VK_QUEUE_FAMILY_IGNORED;

	u32 QueueFamilyPropertiesCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyPropertiesCount, 0);
	std::vector<VkQueueFamilyProperties> QueueFamilyProperties(QueueFamilyPropertiesCount);
	vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &QueueFamilyPropertiesCount, QueueFamilyProperties.data());
	for(u32 FamilyPropertyIndex = 0;
		FamilyPropertyIndex < QueueFamilyPropertiesCount;
		++FamilyPropertyIndex)
	{
		if(QueueFamilyProperties[FamilyPropertyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			Result = FamilyPropertyIndex;
			break;
		}
	}

	return Result;
}

internal VkDevice
CreateDevice(const VkPhysicalDevice& PhysicalDevice, u32* FamilyIndex)
{
	std::vector<const char*> Extensions = 
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
		VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
		VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
		VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
		VK_NV_MESH_SHADER_EXTENSION_NAME,
	};

	std::vector<const char*> Layers = 
	{
	};

	float QueuePriorities[] = {1.0f};
	VkDeviceQueueCreateInfo QueueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	QueueCreateInfo.queueFamilyIndex = *FamilyIndex;
	QueueCreateInfo.queueCount = 1;
	QueueCreateInfo.pQueuePriorities = QueuePriorities;

	VkPhysicalDeviceFeatures2 Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

	VkPhysicalDevice8BitStorageFeatures Features8 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES};
	Features8.storageBuffer8BitAccess = true;
	Features8.uniformAndStorageBuffer8BitAccess = true;

	VkPhysicalDevice16BitStorageFeatures Features16 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES};
	Features16.storageBuffer16BitAccess = true;
	Features16.uniformAndStorageBuffer16BitAccess = true;

	VkPhysicalDeviceMeshShaderFeaturesNV FeaturesNV = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV};
	FeaturesNV.meshShader = true;
	FeaturesNV.taskShader = true;

	VkDeviceCreateInfo DeviceCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	DeviceCreateInfo.ppEnabledExtensionNames = Extensions.data();
	DeviceCreateInfo.enabledExtensionCount = (u32)Extensions.size();
	DeviceCreateInfo.ppEnabledLayerNames = Layers.data();
	DeviceCreateInfo.enabledLayerCount = (u32)Layers.size();
	DeviceCreateInfo.queueCreateInfoCount = 1;
	DeviceCreateInfo.pQueueCreateInfos = &QueueCreateInfo;

	DeviceCreateInfo.pNext = &Features;
	Features.pNext = &Features8;
	Features8.pNext = &Features16;
	Features16.pNext = &FeaturesNV;

	VkDevice Device = 0;
	VkResult ThisResult = vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, 0, &Device);
	VK_CHECK(ThisResult);

	return Device;
}

internal VkFramebuffer
CreateFramebuffer(VkDevice Device, VkRenderPass RenderPass, VkImageView View, u32 Width, u32 Height)
{
	VkFramebufferCreateInfo FramebufferCreateInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	FramebufferCreateInfo.pAttachments = &View;
	FramebufferCreateInfo.attachmentCount = 1;
	FramebufferCreateInfo.width = Width;
	FramebufferCreateInfo.height = Height;
	FramebufferCreateInfo.renderPass = RenderPass;
	FramebufferCreateInfo.layers = 1;
	VkFramebuffer Result = 0;
	vkCreateFramebuffer(Device, &FramebufferCreateInfo, 0, &Result);
	return Result;
}

internal VkImageView
CreateImageView(VkDevice Device, VkImage Image, VkFormat Format)
{
	VkImageViewCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	CreateInfo.format = Format;
	CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	CreateInfo.image = Image;
	CreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	CreateInfo.subresourceRange.layerCount = 1;
	CreateInfo.subresourceRange.levelCount = 1;

	VkImageView Result = 0;
	vkCreateImageView(Device, &CreateInfo, 0, &Result);
	return Result;
}

internal VkSwapchainKHR
CreateSwapchain(VkDevice Device, VkSurfaceKHR Surface, VkSurfaceFormatKHR SurfaceFormat, VkSurfaceCapabilitiesKHR SurfaceCaps, u32 Width, u32 Height, u32* FamilyIndex, VkSwapchainKHR OldSwapchain = 0)
{
	VkCompositeAlphaFlagBitsKHR CompositeAlpha = 
		(SurfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR :
		(SurfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR :
		(SurfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) ? VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR :
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

	VkSwapchainCreateInfoKHR SwapchainCreateInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
	SwapchainCreateInfo.surface = Surface;
	SwapchainCreateInfo.minImageCount = 2;
	SwapchainCreateInfo.imageFormat = SurfaceFormat.format;
	SwapchainCreateInfo.imageColorSpace = SurfaceFormat.colorSpace;
	SwapchainCreateInfo.imageExtent.width = Width;
	SwapchainCreateInfo.imageExtent.height = Height;
	SwapchainCreateInfo.imageArrayLayers = 1;
	SwapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	SwapchainCreateInfo.queueFamilyIndexCount = 1;
	SwapchainCreateInfo.pQueueFamilyIndices = FamilyIndex;
	SwapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	SwapchainCreateInfo.compositeAlpha = CompositeAlpha;
	SwapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	SwapchainCreateInfo.oldSwapchain = OldSwapchain;
	VkSwapchainKHR Swapchain = 0;
	VK_CHECK(vkCreateSwapchainKHR(Device, &SwapchainCreateInfo, 0, &Swapchain));
	return Swapchain;
}

internal void
CreateSwapchain(swapchain& Swapchain, VkRenderPass RenderPass, VkDevice Device, VkSurfaceKHR Surface, VkSurfaceFormatKHR SurfaceFormat, VkSurfaceCapabilitiesKHR SurfaceCaps, u32 Width, u32 Height, u32* FamilyIndex, VkSwapchainKHR OldSwapchain = 0)
{
	Swapchain.Handle = CreateSwapchain(Device, Surface, SurfaceFormat, SurfaceCaps, Width, Height, FamilyIndex, OldSwapchain);
	Swapchain.Width  = Width;
	Swapchain.Height = Height;

	u32 SwapchainImageCount = 0;
	vkGetSwapchainImagesKHR(Device, Swapchain.Handle, &SwapchainImageCount, 0);
	std::vector<VkImage> SwapchainImages(SwapchainImageCount);
	vkGetSwapchainImagesKHR(Device, Swapchain.Handle, &SwapchainImageCount, SwapchainImages.data());
	Swapchain.Images = std::move(SwapchainImages);

	std::vector<VkImageView> SwapchainImageViews(SwapchainImageCount);
	for(u32 ImageViewIndex = 0;
		ImageViewIndex < SwapchainImageViews.size();
		++ImageViewIndex)
	{
		SwapchainImageViews[ImageViewIndex] = CreateImageView(Device, Swapchain.Images[ImageViewIndex], SurfaceFormat.format);
		assert(SwapchainImageViews[ImageViewIndex]);
	}
	Swapchain.ImageViews = std::move(SwapchainImageViews);

	std::vector<VkFramebuffer> Framebuffers(SwapchainImageCount);
	for(u32 FramebufferIndex = 0;
		FramebufferIndex < Framebuffers.size();
		++FramebufferIndex)
	{
		Framebuffers[FramebufferIndex] = CreateFramebuffer(Device, RenderPass, Swapchain.ImageViews[FramebufferIndex], Width, Height);
		assert(Framebuffers[FramebufferIndex]);
	}
	Swapchain.Framebuffers = std::move(Framebuffers);
}

internal void
DestroySwapchain(const swapchain& Swapchain, VkDevice Device)
{
	for(u32 FramebufferIndex = 0;
		FramebufferIndex < Swapchain.Framebuffers.size();
		++FramebufferIndex)
	{
		vkDestroyFramebuffer(Device, Swapchain.Framebuffers[FramebufferIndex], 0);
	}

	for(u32 ImageViewIndex = 0;
		ImageViewIndex < Swapchain.ImageViews.size();
		++ImageViewIndex)
	{
		vkDestroyImageView(Device, Swapchain.ImageViews[ImageViewIndex], 0);
	}

	vkDestroySwapchainKHR(Device, Swapchain.Handle, 0);
}

internal void 
ResizeSwapchain(swapchain& Result, VkRenderPass RenderPass, VkDevice Device, VkSurfaceKHR Surface, VkSurfaceFormatKHR SurfaceFormat, VkSurfaceCapabilitiesKHR SurfaceCaps, u32 Width, u32 Height, u32* FamilyIndex)
{
	VkSwapchainKHR OldSwapchain = Result.Handle;
	swapchain Old = Result;
	CreateSwapchain(Result, RenderPass, Device, Surface, SurfaceFormat, SurfaceCaps, Width, Height, FamilyIndex, OldSwapchain);
	VK_CHECK(vkDeviceWaitIdle(Device));
	DestroySwapchain(Old, Device);
}

internal VkRenderPass
CreateRenderPass(VkDevice Device, VkFormat Format)
{
	VkAttachmentReference ColorAttachments = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

	VkSubpassDescription Subpass = {};
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.colorAttachmentCount = 1;
	Subpass.pColorAttachments = &ColorAttachments;

	VkAttachmentDescription Attachment[1] = {};
	Attachment[0].format = Format;
	Attachment[0].samples = VK_SAMPLE_COUNT_1_BIT;
	Attachment[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	Attachment[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	Attachment[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	Attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachment[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	Attachment[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkRenderPassCreateInfo RenderPassCreateInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	RenderPassCreateInfo.pSubpasses = &Subpass;
	RenderPassCreateInfo.subpassCount = 1;
	RenderPassCreateInfo.pAttachments = Attachment;
	RenderPassCreateInfo.attachmentCount = 1;
	VkRenderPass Result = 0;
	vkCreateRenderPass(Device, &RenderPassCreateInfo, 0, &Result);
	return Result;
}

internal VkSemaphore
CreateSemaphore(VkDevice Device)
{
	VkSemaphoreCreateInfo SemaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	VkSemaphore Semaphore = 0;
	VK_CHECK(vkCreateSemaphore(Device, &SemaphoreCreateInfo, 0, &Semaphore));
	return Semaphore;
}

internal bool
SupportsPresentation(VkPhysicalDevice PhysicalDevice, u32 FamilyIndex)
{
	bool Result = 0;
	Result = vkGetPhysicalDeviceWin32PresentationSupportKHR(PhysicalDevice, FamilyIndex);
	return Result;
}

internal VkPhysicalDevice 
PickPhysicalDevice(const std::vector<VkPhysicalDevice>& PhysicalDevices, u32* FamilyIndex)
{
	VkPhysicalDevice Discrete = 0;
	VkPhysicalDevice Fallback = 0;

	for(u32 PhysicalDeviceIndex = 0;
		PhysicalDeviceIndex < PhysicalDevices.size();
		++PhysicalDeviceIndex)
	{
		VkPhysicalDeviceProperties Props;
		vkGetPhysicalDeviceProperties(PhysicalDevices[PhysicalDeviceIndex], &Props);
		*FamilyIndex = GetGraphicsQueueFamilyIndex(PhysicalDevices[PhysicalDeviceIndex]);

		if(*FamilyIndex == VK_QUEUE_FAMILY_IGNORED)
		{
			continue;
		}

		if(!SupportsPresentation(PhysicalDevices[PhysicalDeviceIndex], *FamilyIndex))
		{
			continue;
		}

		if(!Discrete && Props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			Discrete = PhysicalDevices[PhysicalDeviceIndex];
		}

		if(!Fallback)
		{
			Fallback = PhysicalDevices[PhysicalDeviceIndex];
		}
	}

	return Discrete ? Discrete : Fallback;
}

internal VkShaderModule
LoadShader(VkDevice Device, const char* Path)
{
	VkShaderModule Result = 0;
	FILE* File = fopen(Path, "rb");
	if(File)
	{
		fseek(File, 0, SEEK_END);
		long FileLength = ftell(File);
		fseek(File, 0, SEEK_SET);

		char* Buffer = (char*)malloc(FileLength);
		assert(Buffer);

		size_t ReadSize = fread(Buffer, 1, FileLength, File);
		assert(ReadSize == size_t(FileLength));
		assert(FileLength % 4 == 0);

		VkShaderModuleCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
		CreateInfo.codeSize = FileLength;
		CreateInfo.pCode = reinterpret_cast<const u32*>(Buffer);

		VK_CHECK(vkCreateShaderModule(Device, &CreateInfo, 0, &Result));
	}
	fclose(File);
	return Result;
}

struct shader_layout 
{
	VkPipelineLayout PipelineLayout;
	VkDescriptorSetLayout DescriptorSetLayout;
};

internal shader_layout 
CreatePipelineLayout(VkDevice Device, bool RtxEnabled)
{
	shader_layout Result = {};

	std::vector<VkDescriptorSetLayoutBinding> SetBindings;
	if(RtxEnabled)
	{
		VkDescriptorSetLayoutBinding NewBindings0 = {};
		NewBindings0.binding = 0;
		NewBindings0.descriptorCount = 1;
		NewBindings0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		NewBindings0.stageFlags = VK_SHADER_STAGE_MESH_BIT_NV;

		VkDescriptorSetLayoutBinding NewBindings1 = {};
		NewBindings1.binding = 1;
		NewBindings1.descriptorCount = 1;
		NewBindings1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		NewBindings1.stageFlags = VK_SHADER_STAGE_MESH_BIT_NV;
		SetBindings.push_back(NewBindings0);
		SetBindings.push_back(NewBindings1);
	}
	else
	{
		VkDescriptorSetLayoutBinding NewBindings = {};
		NewBindings.binding = 0;
		NewBindings.descriptorCount = 1;
		NewBindings.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		NewBindings.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		SetBindings.push_back(NewBindings);
	}

	VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; 
	DescriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	DescriptorSetLayoutCreateInfo.bindingCount = (u32)SetBindings.size();
	DescriptorSetLayoutCreateInfo.pBindings = SetBindings.data();

	VK_CHECK(vkCreateDescriptorSetLayout(Device, &DescriptorSetLayoutCreateInfo, 0, &Result.DescriptorSetLayout));

	VkPipelineLayoutCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	CreateInfo.pSetLayouts = &Result.DescriptorSetLayout;
	CreateInfo.setLayoutCount = 1;

	VK_CHECK(vkCreatePipelineLayout(Device, &CreateInfo, 0, &Result.PipelineLayout));

	return Result;
}

internal VkPipeline
CreateGraphicsPipeline(VkDevice Device, VkPipelineCache PipelineCache, shader_layout Layout, VkRenderPass RenderPass, VkShaderModule VS, VkShaderModule FS, bool RtxEnabled)
{
	VkGraphicsPipelineCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

	VkPipelineShaderStageCreateInfo Stages[2] = {};
	Stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	Stages[0].module = VS;
	Stages[0].stage = RtxEnabled ? VK_SHADER_STAGE_MESH_BIT_NV : VK_SHADER_STAGE_VERTEX_BIT;
	Stages[0].pName = "main";
	Stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	Stages[1].module = FS;
	Stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	Stages[1].pName = "main";

	CreateInfo.layout = Layout.PipelineLayout;
	CreateInfo.renderPass = RenderPass;
	CreateInfo.pStages = Stages;
	CreateInfo.stageCount = ArraySize(Stages);

	VkPipelineVertexInputStateCreateInfo VertexInputState = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

	VkPipelineInputAssemblyStateCreateInfo InputAssemblyState = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	InputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineColorBlendStateCreateInfo ColorBlendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	VkPipelineColorBlendAttachmentState ColorAttachmentState = {};
	ColorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	ColorBlendState.pAttachments = &ColorAttachmentState;
	ColorBlendState.attachmentCount = 1;

	VkPipelineDepthStencilStateCreateInfo DepthStencilState = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

	VkPipelineViewportStateCreateInfo ViewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	ViewportState.viewportCount = 1;
	ViewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo RasterizationState = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	RasterizationState.lineWidth = 1.0f;
	RasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

	VkPipelineDynamicStateCreateInfo DynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	VkDynamicState DynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	DynamicState.pDynamicStates = DynamicStates;
	DynamicState.dynamicStateCount = 2;

	VkPipelineMultisampleStateCreateInfo MultisampleState = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	MultisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineTessellationStateCreateInfo TessellationState = {VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};

	CreateInfo.pColorBlendState = &ColorBlendState;
	CreateInfo.pDepthStencilState = &DepthStencilState;
	CreateInfo.pDynamicState = &DynamicState;
	CreateInfo.pInputAssemblyState = &InputAssemblyState;
	CreateInfo.pMultisampleState = &MultisampleState;
	CreateInfo.pRasterizationState = &RasterizationState;
	CreateInfo.pTessellationState = &TessellationState;
	CreateInfo.pVertexInputState = &VertexInputState;
	CreateInfo.pViewportState = &ViewportState;

	VkPipeline Result = 0;
	VK_CHECK(vkCreateGraphicsPipelines(Device, PipelineCache, 1, &CreateInfo, 0, &Result));
	return Result;
}

internal VkSurfaceFormatKHR 
GetSwapchainFormat(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface)
{
	VkSurfaceFormatKHR Result = {};

	u32 FormatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, 0);
	std::vector<VkSurfaceFormatKHR> Formats(FormatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, Formats.data());

	for(u32 FormatIndex = 0;
		FormatIndex < FormatCount;
		++FormatIndex)
	{
		if(Formats[FormatIndex].format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 || Formats[FormatIndex].format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
		{
			return Formats[FormatIndex];
		}
	}

	for(u32 FormatIndex = 0;
		FormatIndex < FormatCount;
		++FormatIndex)
	{
		if(Formats[FormatIndex].format == VK_FORMAT_R8G8B8A8_UNORM || Formats[FormatIndex].format == VK_FORMAT_B8G8R8A8_UNORM)
		{
			return Formats[FormatIndex];
		}
	}

	Result = Formats[0];
	return Result;
}

u32 SelectMemoryType(const VkPhysicalDeviceMemoryProperties& MemoryProperties, u32 MemoryTypeBits, VkMemoryPropertyFlags Flags)
{
	for(u32 PropertyIndex = 0;
		PropertyIndex < MemoryProperties.memoryTypeCount;
		++PropertyIndex)
	{
		if((MemoryTypeBits & (1 << PropertyIndex)) != 0 && (MemoryProperties.memoryTypes[PropertyIndex].propertyFlags & Flags) == Flags)
		{
			return PropertyIndex;
		}
	}

	return ~0u;
}

internal void
CreateBuffer(buffer& Buffer, VkDevice Device, const VkPhysicalDeviceMemoryProperties& MemoryProperties, size_t Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags MemoryFlags)
{
	VkBufferCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	CreateInfo.usage = Usage;
	CreateInfo.size = Size;
	VK_CHECK(vkCreateBuffer(Device, &CreateInfo, 0, &Buffer.Handle));
	Buffer.Size = Size;

	VkMemoryRequirements Requirements;
	vkGetBufferMemoryRequirements(Device, Buffer.Handle, &Requirements);

	u32 MemoryTypeIndex = SelectMemoryType(MemoryProperties, Requirements.memoryTypeBits, MemoryFlags);
	assert(MemoryTypeIndex != ~0u);

	VkMemoryAllocateInfo AllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	AllocateInfo.memoryTypeIndex = MemoryTypeIndex;
	AllocateInfo.allocationSize = Requirements.size;

	VK_CHECK(vkAllocateMemory(Device, &AllocateInfo, 0, &Buffer.Memory));

	vkBindBufferMemory(Device, Buffer.Handle, Buffer.Memory, 0);

	if (MemoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) 
	{
		VK_CHECK(vkMapMemory(Device, Buffer.Memory, 0, Size, 0, &Buffer.Data));
	}
}

internal void
CopyBuffer(buffer& Src, buffer& Dst, const void* Data, size_t Size, VkDevice Device, VkCommandPool CommandPool, VkCommandBuffer CommandBuffer, VkQueue Queue)
{
	assert(Src.Data);
	assert(Src.Size >= Size);
	memcpy(Src.Data, Data, Size);

	VK_CHECK(vkResetCommandPool(Device, CommandPool, 0));
	VkCommandBufferBeginInfo CommandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

	VkBufferCopy Region = {0, 0, VkDeviceSize(Size)};
	vkCmdCopyBuffer(CommandBuffer, Src.Handle, Dst.Handle, 1, &Region);

	VkBufferMemoryBarrier CopyBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
	CopyBarrier.buffer = Dst.Handle;
	CopyBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	CopyBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	CopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	CopyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	CopyBarrier.offset = 0;
	CopyBarrier.size = VK_WHOLE_SIZE;

	vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &CopyBarrier, 0, 0);

	vkEndCommandBuffer(CommandBuffer);

	VkSubmitInfo SubmitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &CommandBuffer;
	VK_CHECK(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

	VK_CHECK(vkDeviceWaitIdle(Device));
}

internal VkQueryPool
CreateQueryPool(VkDevice Device, VkQueryType Type, uint32_t QueryCount)
{
	VkQueryPoolCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
	CreateInfo.queryType = Type;
	CreateInfo.queryCount = QueryCount;
	VkQueryPool Result = 0;
	VK_CHECK(vkCreateQueryPool(Device, &CreateInfo, 0, &Result));
	return Result;
}

internal void
DestroyBuffer(buffer& Buffer, VkDevice Device)
{
	vkFreeMemory(Device, Buffer.Memory, 0);
	vkDestroyBuffer(Device, Buffer.Handle, 0);
}

internal void
PipelineBarrierImage(VkCommandBuffer CommandBuffer, VkImage Image, 
					 VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask, 
					 VkAccessFlags SrcAccess, VkAccessFlags DstAccess, 
					 VkImageLayout SrcImageLayout, VkImageLayout DstImageLayout)
{
	VkImageMemoryBarrier ImageMemoryBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	ImageMemoryBarrier.srcAccessMask = SrcAccess;
	ImageMemoryBarrier.dstAccessMask = DstAccess;
	ImageMemoryBarrier.oldLayout = SrcImageLayout;
	ImageMemoryBarrier.newLayout = DstImageLayout;
	ImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ImageMemoryBarrier.image = Image;
	ImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ImageMemoryBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ImageMemoryBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	vkCmdPipelineBarrier(CommandBuffer, SrcStageMask, DstStageMask, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &ImageMemoryBarrier);
}

LRESULT CALLBACK WindowProc(HWND Wnd, UINT Msg, WPARAM wParam, LPARAM lParam);

int WINAPI 
WinMain(HINSTANCE CurrInst, 
		 HINSTANCE PrevInst, 
		 PSTR Cmd, 
		 int CmdShow) 
{
	LARGE_INTEGER TimeFreq = {};
	QueryPerformanceFrequency(&TimeFreq);
	LARGE_INTEGER BegTime = {}, EndTime = {};

	const char ClassName[] = "Vulkan Win32 Engine";
	WNDCLASS WinClass = {};
	WinClass.lpfnWndProc = WindowProc;
	WinClass.lpszClassName = ClassName;
	WinClass.hInstance = CurrInst;

	RegisterClass(&WinClass);

	u32 WindowWidth = 1280;
	u32 WindowHeight = 720;
	HWND Window = CreateWindowEx(0, ClassName, "Vulkan Engine", WS_OVERLAPPEDWINDOW, 
								 CW_USEDEFAULT, CW_USEDEFAULT, WindowWidth, WindowHeight, 
								 0, 0, CurrInst, 0);

	if(!Window)
	{
		return 1;
	}
	IsRunning = true;
	ShowWindow(Window, CmdShow);

	RECT ClientRect = {};
	GetClientRect(Window, &ClientRect);
	u32 ClientWidth = ClientRect.right - ClientRect.left;
	u32 ClientHeight = ClientRect.bottom - ClientRect.top;

	mesh Mesh;
	bool IsMeshLoaded = LoadMesh(Mesh, "..\\assets\\kitten.obj");
	assert(IsMeshLoaded);
	BuildMeshlets(Mesh);

	VK_CHECK(volkInitialize());
	VkInstance Instance = CreateInstance(ClassName);

	VkDebugReportCallbackEXT Callback = 0;
	VkDebugReportCallbackCreateInfoEXT DebugReportCreateInfo = {VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT};
	DebugReportCreateInfo.pfnCallback = DebugReportCallback;
	DebugReportCreateInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
	VK_CHECK(vkCreateDebugReportCallbackEXT(Instance, &DebugReportCreateInfo, 0, &Callback));

	u32 PhysicalDeviceCount = 0;
	vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, 0);
	std::vector<VkPhysicalDevice> PhysicalDevices(PhysicalDeviceCount);
	VK_CHECK(vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, PhysicalDevices.data()));

	u32 FamilyIndex = 0;
	VkPhysicalDevice PhysicalDevice = PickPhysicalDevice(PhysicalDevices, &FamilyIndex);
	assert(PhysicalDevice);

	VkPhysicalDeviceProperties Props;
	vkGetPhysicalDeviceProperties(PhysicalDevice, &Props);
	assert(Props.limits.timestampPeriod);

	u32 ExtensionCount = 0;
	vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &ExtensionCount, 0);
	std::vector<VkExtensionProperties> SupportedExtensions(ExtensionCount);
	vkEnumerateDeviceExtensionProperties(PhysicalDevice, 0, &ExtensionCount, SupportedExtensions.data());

	for(const auto& Extension : SupportedExtensions)
	{
		if(strcmp(Extension.extensionName, "VK_NV_mesh_shader") == 0)
		{
			IsRtxSupported = true;
			break;
		}
	}
	IsRtxEnabled = IsRtxSupported;

	VkDevice Device = CreateDevice(PhysicalDevice, &FamilyIndex);
	assert(Device);

	u32 Count = 0;
	vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &Count, 0);
	std::vector<VkExtensionProperties> Extensions(Count);
	vkEnumerateDeviceExtensionProperties(PhysicalDevice, nullptr, &Count, Extensions.data());

	VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
	SurfaceCreateInfo.hinstance = CurrInst;
	SurfaceCreateInfo.hwnd = Window;
	VkSurfaceKHR Surface = 0;
	VK_CHECK(vkCreateWin32SurfaceKHR(Instance, &SurfaceCreateInfo, 0, &Surface));
	assert(Surface);

	VkSurfaceCapabilitiesKHR SurfaceCaps;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &SurfaceCaps));

	VkSurfaceFormatKHR SurfaceFormat = GetSwapchainFormat(PhysicalDevice, Surface);

	VkSemaphore AcquireSemaphore = CreateSemaphore(Device);
	assert(AcquireSemaphore);
	VkSemaphore ReleaseSemaphore = CreateSemaphore(Device);
	assert(ReleaseSemaphore);

	VkQueue Queue = 0;
	vkGetDeviceQueue(Device, FamilyIndex, 0, &Queue);
	assert(Queue);

	VkShaderModule ObjectMeshShader = 0;
	if(IsRtxSupported)
	{
		ObjectMeshShader = LoadShader(Device, "..\\shaders\\object.mesh.spv");
		assert(ObjectMeshShader);
	}
	VkShaderModule ObjectVertexShader = LoadShader(Device, "..\\shaders\\object.vert.spv");
	assert(ObjectVertexShader);
	VkShaderModule ObjectFragmentShader = LoadShader(Device, "..\\shaders\\object.frag.spv");
	assert(ObjectFragmentShader);

	VkRenderPass RenderPass = CreateRenderPass(Device, SurfaceFormat.format);
	assert(RenderPass);
	swapchain Swapchain;
	CreateSwapchain(Swapchain, RenderPass, Device, Surface, SurfaceFormat, SurfaceCaps, ClientWidth, ClientHeight, &FamilyIndex);

	VkPipelineCache PipelineCache = 0;
	shader_layout MeshLayout = CreatePipelineLayout(Device, false);
	VkPipeline MeshPipeline = CreateGraphicsPipeline(Device, PipelineCache, MeshLayout, RenderPass, ObjectVertexShader, ObjectFragmentShader, false);
	assert(MeshPipeline);

	shader_layout RtxLayout = {};
	VkPipeline RtxPipeline = 0;
	if(IsRtxSupported)
	{
		RtxLayout = CreatePipelineLayout(Device, true);
		RtxPipeline = CreateGraphicsPipeline(Device, PipelineCache, RtxLayout, RenderPass, ObjectMeshShader, ObjectFragmentShader, true);
		assert(RtxPipeline);
	}

	VkCommandPoolCreateInfo CommandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	CommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	CommandPoolCreateInfo.queueFamilyIndex = FamilyIndex;
	VkCommandPool CommandPool = 0;
	vkCreateCommandPool(Device, &CommandPoolCreateInfo, 0, &CommandPool);
	assert(CommandPool);

	VkCommandBufferAllocateInfo CommandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	CommandBufferAllocateInfo.commandBufferCount = 1;
	CommandBufferAllocateInfo.commandPool = CommandPool;
	CommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VkCommandBuffer CommandBuffer = 0;
	vkAllocateCommandBuffers(Device, &CommandBufferAllocateInfo, &CommandBuffer);

	VkQueryPool QueryPool = CreateQueryPool(Device, VK_QUERY_TYPE_TIMESTAMP, 128);
	assert(QueryPool);

	VkPhysicalDeviceMemoryProperties MemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemoryProperties);

	buffer ScratchBuffer = {};
	CreateBuffer(ScratchBuffer, Device, MemoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	buffer VertexBuffer = {}, IndexBuffer = {}, MeshletBuffer = {};
	CreateBuffer(VertexBuffer, Device, MemoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CopyBuffer(ScratchBuffer, VertexBuffer, Mesh.Vertices.data(), sizeof(vertex) * Mesh.Vertices.size(), Device, CommandPool, CommandBuffer, Queue);

	CreateBuffer(IndexBuffer, Device, MemoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CopyBuffer(ScratchBuffer, IndexBuffer, Mesh.Indices.data(), sizeof(u32) * Mesh.Indices.size(), Device, CommandPool, CommandBuffer, Queue);

	if(IsRtxSupported)
	{
		CreateBuffer(MeshletBuffer, Device, MemoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CopyBuffer(ScratchBuffer, MeshletBuffer, Mesh.Meshlets.data(), sizeof(meshlet) * Mesh.Meshlets.size(), Device, CommandPool, CommandBuffer, Queue);
	}

	double CpuAvgTime = 0;
	double GpuAvgTime = 0;

	while(IsRunning)
	{
		DispatchMessages();
		QueryPerformanceCounter(&BegTime);

		VkSurfaceCapabilitiesKHR ResizeCaps;
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &ResizeCaps));
		if(ResizeCaps.currentExtent.width != Swapchain.Width || ResizeCaps.currentExtent.height != Swapchain.Height)
		{
			ResizeSwapchain(Swapchain, RenderPass, Device, Surface, SurfaceFormat, SurfaceCaps, ResizeCaps.currentExtent.width, ResizeCaps.currentExtent.height, &FamilyIndex);
		}

		u32 ImageIndex = 0;
		VK_CHECK(vkAcquireNextImageKHR(Device, Swapchain.Handle, ~0ull, AcquireSemaphore, VK_NULL_HANDLE, &ImageIndex));

		VK_CHECK(vkResetCommandPool(Device, CommandPool, 0));

		VkCommandBufferBeginInfo CommandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

		vkCmdResetQueryPool(CommandBuffer, QueryPool, 0, 128);
		vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, QueryPool, 0);

		PipelineBarrierImage(CommandBuffer, Swapchain.Images[ImageIndex], VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		VkClearValue ClearColor = {};
		ClearColor.color = {48./255., 25/255., 86/255., 1};

		VkRenderPassBeginInfo RenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
		RenderPassBeginInfo.framebuffer = Swapchain.Framebuffers[ImageIndex];
		RenderPassBeginInfo.renderPass = RenderPass;
		RenderPassBeginInfo.renderArea.extent.width = Swapchain.Width;
		RenderPassBeginInfo.renderArea.extent.height = Swapchain.Height;
		RenderPassBeginInfo.clearValueCount = 1;
		RenderPassBeginInfo.pClearValues = &ClearColor;
		vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport Viewport = {0, (float)Swapchain.Height, (float)Swapchain.Width, -(float)Swapchain.Height, 0, 1};
		VkRect2D Scissor = {{0, 0}, {Swapchain.Width, Swapchain.Height}};
		vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
		vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

		VkDescriptorBufferInfo VertexBufferInfo = {};
		VertexBufferInfo.buffer = VertexBuffer.Handle;
		VertexBufferInfo.offset = 0;
		VertexBufferInfo.range = VertexBuffer.Size;

		if(IsRtxEnabled)
		{
			vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RtxPipeline);

			VkDescriptorBufferInfo MeshletBufferInfo = {};
			MeshletBufferInfo.buffer = MeshletBuffer.Handle;
			MeshletBufferInfo.offset = 0;
			MeshletBufferInfo.range = MeshletBuffer.Size;

			VkWriteDescriptorSet DescriptorSets[2] = {};
			DescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			DescriptorSets[0].dstBinding = 0;
			DescriptorSets[0].descriptorCount = 1;
			DescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			DescriptorSets[0].pBufferInfo = &VertexBufferInfo;
			DescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			DescriptorSets[1].dstBinding = 1;
			DescriptorSets[1].descriptorCount = 1;
			DescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			DescriptorSets[1].pBufferInfo = &MeshletBufferInfo;

			vkCmdPushDescriptorSetKHR(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RtxLayout.PipelineLayout, 0, 2, DescriptorSets);
			vkCmdDrawMeshTasksNV(CommandBuffer, u32(Mesh.Meshlets.size()), 0);
		}
		else
		{
			vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, MeshPipeline);

			VkWriteDescriptorSet DescriptorSets[1] = {};
			DescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			DescriptorSets[0].dstBinding = 0;
			DescriptorSets[0].descriptorCount = 1;
			DescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			DescriptorSets[0].pBufferInfo = &VertexBufferInfo;

			vkCmdPushDescriptorSetKHR(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, MeshLayout.PipelineLayout, 0, 1, DescriptorSets);
			vkCmdBindIndexBuffer(CommandBuffer, IndexBuffer.Handle, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(CommandBuffer, u32(Mesh.Indices.size()), 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(CommandBuffer);
		PipelineBarrierImage(CommandBuffer, Swapchain.Images[ImageIndex], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, QueryPool, 1);

		vkEndCommandBuffer(CommandBuffer);

		VkPipelineStageFlags SubmitStageFlag = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo SubmitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
		SubmitInfo.pWaitDstStageMask = &SubmitStageFlag;
		SubmitInfo.commandBufferCount = 1;
		SubmitInfo.pCommandBuffers = &CommandBuffer;
		SubmitInfo.waitSemaphoreCount = 1;
		SubmitInfo.pWaitSemaphores = &AcquireSemaphore;
		SubmitInfo.signalSemaphoreCount = 1;
		SubmitInfo.pSignalSemaphores = &ReleaseSemaphore;
		vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE);

		VkPresentInfoKHR PresentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
		PresentInfo.waitSemaphoreCount = 1;
		PresentInfo.pWaitSemaphores = &ReleaseSemaphore;
		PresentInfo.swapchainCount = 1;
		PresentInfo.pSwapchains = &Swapchain.Handle;
		PresentInfo.pImageIndices = &ImageIndex;
		vkQueuePresentKHR(Queue, &PresentInfo);

		vkDeviceWaitIdle(Device);

		u64 QueryResults[2] = {};
		vkGetQueryPoolResults(Device, QueryPool, 0, ArraySize(QueryResults), sizeof(QueryResults), QueryResults, sizeof(QueryResults[0]), VK_QUERY_RESULT_64_BIT);

		QueryPerformanceCounter(&EndTime);

		CpuAvgTime = CpuAvgTime * 0.75f + (float)(EndTime.QuadPart - BegTime.QuadPart) / (float)TimeFreq.QuadPart * 1000.0f * 0.25f;
		GpuAvgTime = GpuAvgTime * 0.75f + (float)(QueryResults[1] - QueryResults[0]) * (float)Props.limits.timestampPeriod * 1.e-6f * 0.25f;

		char Title[256];
		sprintf(Title, "%s; Vulkan Engine - cpu: %.2f ms, gpu: %.2f ms; Triangles Count - %llu; Meshlets Count - %llu", 
				IsRtxEnabled ? "RTX Is Enabled" : "RTX Is Disabled",
				CpuAvgTime, 
			    GpuAvgTime,
				Mesh.Indices.size() / 3,
				Mesh.Meshlets.size());
		SetWindowTextA(Window, Title);
	}

	if(IsRtxSupported)
	{
		DestroyBuffer(MeshletBuffer, Device);
	}

	DestroyBuffer(VertexBuffer, Device);
	DestroyBuffer(IndexBuffer, Device);
	DestroyBuffer(ScratchBuffer, Device);

	vkDestroyQueryPool(Device, QueryPool, 0);

	vkDestroyCommandPool(Device, CommandPool, 0);

	DestroySwapchain(Swapchain, Device);

	vkDestroyRenderPass(Device, RenderPass, 0);

	vkDestroyPipeline(Device, MeshPipeline, 0);
	vkDestroyDescriptorSetLayout(Device, MeshLayout.DescriptorSetLayout, 0);
	vkDestroyPipelineLayout(Device, MeshLayout.PipelineLayout, 0);

	if(IsRtxSupported)
	{
		vkDestroyPipeline(Device, RtxPipeline, 0);
		vkDestroyDescriptorSetLayout(Device, RtxLayout.DescriptorSetLayout, 0);
		vkDestroyPipelineLayout(Device, RtxLayout.PipelineLayout, 0);
		vkDestroyShaderModule(Device, ObjectMeshShader, 0);
	}

	vkDestroyShaderModule(Device, ObjectFragmentShader, 0);
	vkDestroyShaderModule(Device, ObjectVertexShader, 0);

	vkDestroySemaphore(Device, AcquireSemaphore, 0);
	vkDestroySemaphore(Device, ReleaseSemaphore, 0);

	vkDestroySurfaceKHR(Instance, Surface, 0);
	vkDestroyDevice(Device, 0);
	vkDestroyDebugReportCallbackEXT(Instance, Callback, 0);
	vkDestroyInstance(Instance, 0);

	return 0;
}

LRESULT CALLBACK WindowProc(HWND Wnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	LRESULT Result = 0;
	switch(Msg)
	{
		case WM_CLOSE:
		{
			if (MessageBox(Wnd, "Do you really want to quit", "Exit", MB_OKCANCEL) == IDOK) 
			{
				IsRunning = false;
			}
		} break;
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		} break;
		default:
		{
			Result = DefWindowProc(Wnd, Msg, wParam, lParam);
		} break;
	}

	return Result;
}

// NOTE: Only when console output is needed
// Otherwise it is directly call WinMain
int main(int argc, char* argv[])
{
	WinMain(NULL, NULL, argv[0], SW_SHOWNORMAL);
}
