#include "intrinsics.h"

struct alignas(16) draw_cull_data
{
	glm::vec4 Frustrum[6];
	u32 LodEnabled;
	u32 CullEnabled;
};

struct alignas(16) depth_reduce_data
{
	glm::vec2 Dims;
};

VkBool32 DebugReportCallback(VkDebugReportFlagsEXT Flags, VkDebugReportObjectTypeEXT ObjectType, u64 Object, size_t Location, s32 MessageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	const char* ErrorType = (Flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ? "Error" :
							(Flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) ? "Warning" : 
							(Flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) ? "Performance Warning" :
							(Flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) ? "Debug" : "Info";

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
#if VK_DEBUG
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
		"VK_KHR_shader_non_semantic_info",
		VK_KHR_8BIT_STORAGE_EXTENSION_NAME,
		VK_KHR_16BIT_STORAGE_EXTENSION_NAME,
		VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
	};
	if(IsRtxSupported)
	{
		Extensions.push_back(VK_NV_MESH_SHADER_EXTENSION_NAME);
	}

	std::vector<const char*> Layers = 
	{
	};

	float QueuePriorities[] = {1.0f};
	VkDeviceQueueCreateInfo QueueCreateInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	QueueCreateInfo.queueFamilyIndex = *FamilyIndex;
	QueueCreateInfo.queueCount = 1;
	QueueCreateInfo.pQueuePriorities = QueuePriorities;

	VkPhysicalDeviceFeatures2 Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	Features.features.multiDrawIndirect = true;
	Features.features.pipelineStatisticsQuery = true;

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
	if(IsRtxSupported)
	{
		Features16.pNext = &FeaturesNV;
	}

	VkDevice Device = 0;
	VkResult ThisResult = vkCreateDevice(PhysicalDevice, &DeviceCreateInfo, 0, &Device);
	VK_CHECK(ThisResult);

	return Device;
}

internal VkRenderPass
CreateRenderPass(VkDevice Device, VkFormat Format, VkFormat DepthFormat, bool IsLate)
{
	VkAttachmentReference ColorAttachments = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference DepthAttachments = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	VkSubpassDescription Subpass = {};
	Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	Subpass.colorAttachmentCount = 1;
	Subpass.pColorAttachments = &ColorAttachments;
	Subpass.pDepthStencilAttachment = &DepthAttachments;

	VkAttachmentDescription Attachment[2] = {};
	Attachment[0].format = Format;
	Attachment[0].samples = VK_SAMPLE_COUNT_1_BIT;
	Attachment[0].loadOp = IsLate ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
	Attachment[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	Attachment[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	Attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachment[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	Attachment[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	Attachment[1].format = DepthFormat;
	Attachment[1].samples = VK_SAMPLE_COUNT_1_BIT;
	Attachment[1].loadOp = IsLate ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
	Attachment[1].storeOp = IsLate ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
	Attachment[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	Attachment[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	Attachment[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	Attachment[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkRenderPassCreateInfo RenderPassCreateInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	RenderPassCreateInfo.pSubpasses = &Subpass;
	RenderPassCreateInfo.subpassCount = 1;
	RenderPassCreateInfo.pAttachments = Attachment;
	RenderPassCreateInfo.attachmentCount = 2;

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

	VK_CHECK(vkBindBufferMemory(Device, Buffer.Handle, Buffer.Memory, 0));

	if (MemoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) 
	{
		VK_CHECK(vkMapMemory(Device, Buffer.Memory, 0, Size, 0, &Buffer.Data));
	}
}

internal VkBufferMemoryBarrier
CreateBufferBarrier(VkBuffer Handle, VkAccessFlags SrcAccess, VkAccessFlags DstAccess, VkDeviceSize Offset = 0, VkDeviceSize Size = VK_WHOLE_SIZE)
{
	VkBufferMemoryBarrier CopyBarrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
	CopyBarrier.buffer = Handle;
	CopyBarrier.srcAccessMask = SrcAccess;
	CopyBarrier.dstAccessMask = DstAccess;
	CopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	CopyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	CopyBarrier.offset = Offset;
	CopyBarrier.size = Size;

	return CopyBarrier;
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

	VkBufferMemoryBarrier CopyBarrier = CreateBufferBarrier(Dst.Handle, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
	vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &CopyBarrier, 0, 0);

	vkEndCommandBuffer(CommandBuffer);

	VkSubmitInfo SubmitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	SubmitInfo.commandBufferCount = 1;
	SubmitInfo.pCommandBuffers = &CommandBuffer;
	VK_CHECK(vkQueueSubmit(Queue, 1, &SubmitInfo, VK_NULL_HANDLE));

	VK_CHECK(vkDeviceWaitIdle(Device));
}

internal void
DestroyBuffer(buffer& Buffer, VkDevice Device)
{
	vkFreeMemory(Device, Buffer.Memory, 0);
	vkDestroyBuffer(Device, Buffer.Handle, 0);
}

internal void
CreateImage(image& Result, VkDevice Device, const VkPhysicalDeviceMemoryProperties& MemoryProperties, u32 Width, u32 Height, VkFormat Format, VkImageUsageFlags Usage, u32 MipLevels = 1)
{
	VkImageCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	CreateInfo.format = Format;
	CreateInfo.imageType = VK_IMAGE_TYPE_2D;
	CreateInfo.extent = {Width, Height, 1};
	CreateInfo.mipLevels = MipLevels;
	CreateInfo.arrayLayers = 1;
	CreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	CreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	CreateInfo.usage = Usage;
	CreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK(vkCreateImage(Device, &CreateInfo, 0, &Result.Handle));

	VkMemoryRequirements Requirements;
	vkGetImageMemoryRequirements(Device, Result.Handle, &Requirements);

	u32 MemoryTypeIndex = SelectMemoryType(MemoryProperties, Requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	assert(MemoryTypeIndex != ~0u);

	VkMemoryAllocateInfo AllocateInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	AllocateInfo.memoryTypeIndex = MemoryTypeIndex;
	AllocateInfo.allocationSize = Requirements.size;

	VK_CHECK(vkAllocateMemory(Device, &AllocateInfo, 0, &Result.Memory));

	VK_CHECK(vkBindImageMemory(Device, Result.Handle, Result.Memory, 0));

	Result.View = CreateImageView(Device, Result.Handle, Format, 0, MipLevels);
}

internal void
DestroyImage(image& Image, VkDevice& Device)
{
	vkFreeMemory(Device, Image.Memory, 0);
	vkDestroyImageView(Device, Image.View, 0);
	vkDestroyImage(Device, Image.Handle, 0);
}

VkSampler CreateSampler(VkDevice Device)
{
	VkSamplerCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
	CreateInfo.minLod = 0;
	CreateInfo.maxLod = 16;
	CreateInfo.magFilter = VK_FILTER_NEAREST;
	CreateInfo.minFilter = VK_FILTER_NEAREST;
	CreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	CreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	CreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	CreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

	VkSampler Result = 0;
	vkCreateSampler(Device, &CreateInfo, 0, &Result);
	return Result;
}

internal VkQueryPool
CreateQueryPool(VkDevice Device, VkQueryType Type, uint32_t QueryCount)
{
	VkQueryPoolCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
	CreateInfo.queryType = Type;
	CreateInfo.queryCount = QueryCount;

	if(Type == VK_QUERY_TYPE_PIPELINE_STATISTICS)
	{
		CreateInfo.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;
	}

	VkQueryPool Result = 0;
	VK_CHECK(vkCreateQueryPool(Device, &CreateInfo, 0, &Result));
	return Result;
}

internal VkImageMemoryBarrier
CreateImageBarrier(VkImage Image, VkAccessFlags SrcAccess, VkAccessFlags DstAccess, 
				   VkImageLayout SrcImageLayout, VkImageLayout DstImageLayout, VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT)
{
	VkImageMemoryBarrier ImageMemoryBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	ImageMemoryBarrier.srcAccessMask = SrcAccess;
	ImageMemoryBarrier.dstAccessMask = DstAccess;
	ImageMemoryBarrier.oldLayout = SrcImageLayout;
	ImageMemoryBarrier.newLayout = DstImageLayout;
	ImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	ImageMemoryBarrier.image = Image;
	ImageMemoryBarrier.subresourceRange.aspectMask = AspectMask;
	ImageMemoryBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	ImageMemoryBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	return ImageMemoryBarrier;
}

internal void
ImageBarrier(VkCommandBuffer CommandBuffer, 
					 VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask,
					 const std::vector<VkImageMemoryBarrier>& ImageMemoryBarriers)
{
	vkCmdPipelineBarrier(CommandBuffer, SrcStageMask, DstStageMask, 
						 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 
						 (u32)ImageMemoryBarriers.size(), ImageMemoryBarriers.data());
}

glm::mat4x4 GetProjection(float FovY, float Aspect, float ZNear)
{
	float f = 1.0f / tanf(0.5f * FovY);
	glm::mat4x4 Proj(0);

	Proj[0][0] = Aspect * f;
	Proj[1][1] = f;
	Proj[2][2] = 0.0f;
	Proj[2][3] = 1.0f;
	Proj[3][2] = ZNear;

	return Proj;
}

glm::vec4 NormalizePlane(glm::vec4 P)
{
	return P / glm::length(glm::vec3(P));
}

u32 GetImageMipLevels(u32 Width, u32 Height)
{
	u32 Result = 0;

	while(Width > 1 && Height > 1)
	{
		Result++;
		Width  /= 2;
		Height /= 2;
	}

	return Result;
}

u32 GetGroupCount(u32 ThreadCount, u32 LocalSize)
{
	return (ThreadCount + LocalSize - 1) / LocalSize;
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

	geometry Geometries;
	bool IsMeshLoaded = LoadMesh(Geometries, "..\\assets\\kitten.obj", true);
	assert(IsMeshLoaded);
	//IsMeshLoaded = LoadMesh(Geometries, "..\\assets\\f22.obj", true);
	//assert(IsMeshLoaded);

	VK_CHECK(volkInitialize());
	VkInstance Instance = CreateInstance(ClassName);

	VkDebugReportCallbackEXT Callback = 0;
	VkDebugReportCallbackCreateInfoEXT DebugReportCreateInfo = {VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT};
	DebugReportCreateInfo.pfnCallback = DebugReportCallback;
	DebugReportCreateInfo.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
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

	shader ObjectMeshShader = {};
	shader ObjectTaskShader = {};
	if(IsRtxSupported)
	{
		LoadShader(ObjectMeshShader, Device, "..\\shaders\\object.mesh.spv");
		LoadShader(ObjectTaskShader, Device, "..\\shaders\\object.task.spv");
	}
	shader ObjectVertexShader = {};
	LoadShader(ObjectVertexShader, Device, "..\\shaders\\object.vert.spv");
	shader ObjectFragmentShader = {};
	LoadShader(ObjectFragmentShader, Device, "..\\shaders\\object.frag.spv");
	shader CommandComputeShader = {};
	LoadShader(CommandComputeShader, Device, "..\\shaders\\cmd.comp.spv");
	shader DepthReduceComputeShader = {};
	LoadShader(DepthReduceComputeShader, Device, "..\\shaders\\depth_reduce.comp.spv");

	VkRenderPass RenderPass = CreateRenderPass(Device, SurfaceFormat.format, VK_FORMAT_D32_SFLOAT, false);
	assert(RenderPass);
	VkRenderPass RenderPassLate = CreateRenderPass(Device, SurfaceFormat.format, VK_FORMAT_D32_SFLOAT, true);
	assert(RenderPassLate);
	swapchain Swapchain;
	CreateSwapchain(Swapchain, RenderPass, Device, Surface, SurfaceFormat, SurfaceCaps, ClientWidth, ClientHeight, &FamilyIndex);

	program ComputeProgram = CreateProgram(Device, VK_PIPELINE_BIND_POINT_COMPUTE, {&CommandComputeShader}, sizeof(draw_cull_data));
	program DepthReduceProgram = CreateProgram(Device, VK_PIPELINE_BIND_POINT_COMPUTE, {&DepthReduceComputeShader}, sizeof(depth_reduce_data));
	program MeshProgram = CreateProgram(Device, VK_PIPELINE_BIND_POINT_GRAPHICS, {&ObjectVertexShader, &ObjectFragmentShader}, sizeof(globals));

	VkPipelineCache PipelineCache = 0;
	VkPipeline DrawCmdPipeline = CreateComputePipeline(Device, PipelineCache, ComputeProgram.Layout, CommandComputeShader);
	assert(DrawCmdPipeline);
	VkPipeline DepthReducePipeline = CreateComputePipeline(Device, PipelineCache, DepthReduceProgram.Layout, DepthReduceComputeShader);
	assert(DepthReducePipeline);

	VkPipeline MeshPipeline = CreateGraphicsPipeline(Device, PipelineCache, MeshProgram.Layout, RenderPass, {&ObjectVertexShader, &ObjectFragmentShader});
	assert(MeshPipeline);

	program RtxProgram = {};
	VkPipeline RtxPipeline = 0;
	if(IsRtxSupported)
	{
		RtxProgram = CreateProgram(Device, VK_PIPELINE_BIND_POINT_GRAPHICS, {&ObjectTaskShader, &ObjectMeshShader, &ObjectFragmentShader}, sizeof(globals));
		
		RtxPipeline = CreateGraphicsPipeline(Device, PipelineCache, RtxProgram.Layout, RenderPass, {&ObjectTaskShader, &ObjectMeshShader, &ObjectFragmentShader});
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

	VkQueryPool TimestampsQueryPool = CreateQueryPool(Device, VK_QUERY_TYPE_TIMESTAMP, 128);
	assert(TimestampsQueryPool);

	VkQueryPool PipelineQueryPool = CreateQueryPool(Device, VK_QUERY_TYPE_PIPELINE_STATISTICS, 1);
	assert(PipelineQueryPool);

	VkPhysicalDeviceMemoryProperties MemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &MemoryProperties);

	buffer ScratchBuffer = {};
	CreateBuffer(ScratchBuffer, Device, MemoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	buffer VertexBuffer = {}, IndexBuffer = {}, MeshBuffer = {}, MeshletBuffer = {}, MeshletDataBuffer = {}, DrawBuffer = {}, DrawCommandBuffer = {};
	CreateBuffer(VertexBuffer, Device, MemoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CopyBuffer(ScratchBuffer, VertexBuffer, Geometries.Vertices.data(), sizeof(vertex) * Geometries.Vertices.size(), Device, CommandPool, CommandBuffer, Queue);

	CreateBuffer(IndexBuffer, Device, MemoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CopyBuffer(ScratchBuffer, IndexBuffer, Geometries.Indices.data(), sizeof(u32) * Geometries.Indices.size(), Device, CommandPool, CommandBuffer, Queue);

	CreateBuffer(MeshBuffer, Device, MemoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CopyBuffer(ScratchBuffer, MeshBuffer, Geometries.Meshes.data(), sizeof(mesh) * Geometries.Meshes.size(), Device, CommandPool, CommandBuffer, Queue);

	if(IsRtxSupported)
	{
		CreateBuffer(MeshletBuffer, Device, MemoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CopyBuffer(ScratchBuffer, MeshletBuffer, Geometries.Meshlets.data(), sizeof(meshlet) * Geometries.Meshlets.size(), Device, CommandPool, CommandBuffer, Queue);
	}

	VkSampler DepthSampler = CreateSampler(Device);
	assert(DepthSampler);

	buffer DrawCommandCountBuffer = {};
	CreateBuffer(DrawCommandCountBuffer, Device, MemoryProperties, 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkFramebuffer TargetFramebuffer = 0;
	image ColorTarget = {}, DepthTarget = {}, DepthPyramid = {};

	double CpuAvgTime = 0;
	double GpuAvgTime = 0;

	srand(512);
	
	u64 TriangleCount = 0;
	u32 DrawCount = 1000000;
	DrawCount = (DrawCount + 31) & ~31;

	std::vector<mesh_offset> DrawOffsets(DrawCount);

	float SceneRadius  = 500;
	float DrawDistance = 100;
	for(u32 DrawIndex = 0;
		DrawIndex < DrawCount;
		++DrawIndex)
	{
		u32 MeshIndex = rand() % Geometries.Meshes.size();
		const mesh& Mesh = Geometries.Meshes[MeshIndex];

		DrawOffsets[DrawIndex].Pos[0] =  float(rand()) / RAND_MAX * SceneRadius * 2 - SceneRadius;
		DrawOffsets[DrawIndex].Pos[1] =  float(rand()) / RAND_MAX * SceneRadius * 2 - SceneRadius;
		DrawOffsets[DrawIndex].Pos[2] =  float(rand()) / RAND_MAX * SceneRadius * 2 - SceneRadius;
		DrawOffsets[DrawIndex].Scale  = (float(rand()) / RAND_MAX) * 10;

		glm::vec3 Axis{(float(rand()) / RAND_MAX) * 2 - 1, (float(rand()) / RAND_MAX) * 2 - 1, (float(rand()) / RAND_MAX) * 2 - 1};
		float Angle = glm::radians((float(rand()) / RAND_MAX) * 90.0f);

		DrawOffsets[DrawIndex].Orient = glm::rotate(glm::quat(1, 0, 0, 0), Angle, Axis);

		DrawOffsets[DrawIndex].VertexOffset = Mesh.VertexOffset;
		DrawOffsets[DrawIndex].MeshIndex    = MeshIndex;
	}

	CreateBuffer(DrawBuffer, Device, MemoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CopyBuffer(ScratchBuffer, DrawBuffer, DrawOffsets.data(), sizeof(mesh_offset) * DrawOffsets.size(), Device, CommandPool, CommandBuffer, Queue);

	CreateBuffer(DrawCommandBuffer, Device, MemoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkImageView DepthPyramidMips[16] = {};
	u32 DepthPyramidLevels = 0;

	draw_cull_data DrawCullData = {};
	u32 ImageIndex = 0;
	while(IsRunning)
	{
		DispatchMessages();
		QueryPerformanceCounter(&BegTime);
		DrawCullData.CullEnabled = IsCullEnabled;
		DrawCullData.LodEnabled = IsLodEnabled;

		if(ResizeSwapchain(Swapchain, PhysicalDevice, RenderPass, Device, Surface, SurfaceFormat, SurfaceCaps, &FamilyIndex) || !TargetFramebuffer)
		{
			if(ColorTarget.Handle)
				DestroyImage(ColorTarget, Device);
			if(DepthTarget.Handle)
				DestroyImage(DepthTarget, Device);
			if(DepthPyramid.Handle)
			{
				for(u32 DepthPyramidIdx = 0;
					DepthPyramidIdx < DepthPyramidLevels;
					++DepthPyramidIdx)
				{
					vkDestroyImageView(Device, DepthPyramidMips[DepthPyramidIdx], 0);
				}
				DestroyImage(DepthPyramid, Device);
			}
			if(TargetFramebuffer)
				vkDestroyFramebuffer(Device, TargetFramebuffer, 0);

			CreateImage(ColorTarget, Device, MemoryProperties, Swapchain.Width, Swapchain.Height, SurfaceFormat.format, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
			CreateImage(DepthTarget, Device, MemoryProperties, Swapchain.Width, Swapchain.Height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

			DepthPyramidLevels = GetImageMipLevels(Swapchain.Width / 2, Swapchain.Height / 2);
			GlobalDepthPyramidLevels = DepthPyramidLevels;

			CreateImage(DepthPyramid, Device, MemoryProperties, Swapchain.Width / 2, Swapchain.Height / 2, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, DepthPyramidLevels);

			for(u32 DepthPyramidIdx = 0;
				DepthPyramidIdx < DepthPyramidLevels;
				++DepthPyramidIdx)
			{
				DepthPyramidMips[DepthPyramidIdx] = CreateImageView(Device, DepthPyramid.Handle, VK_FORMAT_R32_SFLOAT, DepthPyramidIdx, 1);
				assert(DepthPyramidMips[DepthPyramidIdx]);
			}

			TargetFramebuffer = CreateFramebuffer(Device, RenderPass, ColorTarget.View, DepthTarget.View, Swapchain.Width, Swapchain.Height);
		}

		VK_CHECK(vkAcquireNextImageKHR(Device, Swapchain.Handle, ~0ull, AcquireSemaphore, VK_NULL_HANDLE, &ImageIndex));

		VK_CHECK(vkResetCommandPool(Device, CommandPool, 0));

		VkCommandBufferBeginInfo CommandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		CommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(CommandBuffer, &CommandBufferBeginInfo);

		vkCmdResetQueryPool(CommandBuffer, TimestampsQueryPool, 0, 128);
		vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampsQueryPool, 0);

		glm::mat4x4 Projection = GetProjection(70.0f, (float)Swapchain.Height / (float)Swapchain.Width, 1.f);
		{
			glm::mat4 ProjectionT = glm::transpose(Projection);
			DrawCullData.Frustrum[0] = NormalizePlane(ProjectionT[3] + ProjectionT[0]);
			DrawCullData.Frustrum[1] = NormalizePlane(ProjectionT[3] - ProjectionT[0]);
			DrawCullData.Frustrum[2] = NormalizePlane(ProjectionT[3] + ProjectionT[1]);
			DrawCullData.Frustrum[3] = NormalizePlane(ProjectionT[3] - ProjectionT[1]);
			DrawCullData.Frustrum[4] = NormalizePlane(ProjectionT[3] - ProjectionT[2]);
			DrawCullData.Frustrum[5] = glm::vec4(0, 0, -1, DrawDistance);

			vkCmdFillBuffer(CommandBuffer, DrawCommandCountBuffer.Handle, 0, 4, 0);
			VkBufferMemoryBarrier ZeroInitBarrier = CreateBufferBarrier(DrawCommandCountBuffer.Handle, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 1, &ZeroInitBarrier, 0, 0);

			vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, DrawCmdPipeline);

			descriptor_template ComputeDescriptors[] = {{DrawBuffer.Handle}, {MeshBuffer.Handle}, {DrawCommandBuffer.Handle}, {DrawCommandCountBuffer.Handle}};
			vkCmdPushDescriptorSetWithTemplateKHR(CommandBuffer, ComputeProgram.DescriptorTemplate, ComputeProgram.Layout, 0, ComputeDescriptors);

			vkCmdPushConstants(CommandBuffer, ComputeProgram.Layout, ComputeProgram.Stages, 0, sizeof(draw_cull_data), &DrawCullData);
			
			vkCmdDispatch(CommandBuffer, GetGroupCount(DrawOffsets.size(), CommandComputeShader.LocalSizeX), 1, 1);

			VkBufferMemoryBarrier CmdEndBufferBarrier = CreateBufferBarrier(DrawCommandBuffer.Handle, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT);
			vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, 0, 0, 0, 1, &CmdEndBufferBarrier, 0, 0);
		}

		std::vector<VkImageMemoryBarrier> ImageBeginRenderBarriers = 
		{
			CreateImageBarrier(ColorTarget.Handle, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
			CreateImageBarrier(DepthTarget.Handle, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT),
		};

		ImageBarrier(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, ImageBeginRenderBarriers);

		vkCmdResetQueryPool(CommandBuffer, PipelineQueryPool, 0, 1);
		vkCmdBeginQuery(CommandBuffer, PipelineQueryPool, 0, 0);

		VkViewport Viewport = {0, (float)Swapchain.Height, (float)Swapchain.Width, -(float)Swapchain.Height, 0, 1};
		VkRect2D Scissor = {{0, 0}, {Swapchain.Width, Swapchain.Height}};
		vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);
		vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

		VkClearValue ClearColor[2] = {};
		ClearColor[0].color = {48./255., 25/255., 86/255., 1};
		ClearColor[1].depthStencil = {0, 0};

		VkRenderPassBeginInfo RenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
		RenderPassBeginInfo.framebuffer = TargetFramebuffer;
		RenderPassBeginInfo.renderPass = RenderPass;
		RenderPassBeginInfo.renderArea.extent.width = Swapchain.Width;
		RenderPassBeginInfo.renderArea.extent.height = Swapchain.Height;
		RenderPassBeginInfo.clearValueCount = 2;
		RenderPassBeginInfo.pClearValues = ClearColor;
		vkCmdBeginRenderPass(CommandBuffer, &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		globals Globals = {};
		Globals.Projection = Projection;

		if(IsRtxEnabled)
		{
			vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, RtxPipeline);

			VkDescriptorBufferInfo MeshletBufferInfo = {};
			MeshletBufferInfo.buffer = MeshletBuffer.Handle;
			MeshletBufferInfo.offset = 0;
			MeshletBufferInfo.range = MeshletBuffer.Size;

			descriptor_template DescriptorInfo[] = {{DrawBuffer.Handle, 0, DrawBuffer.Size},
													{DrawCommandBuffer.Handle, 0, DrawCommandBuffer.Size},
													{VertexBuffer.Handle, 0, VertexBuffer.Size}, 
													{MeshletBuffer.Handle, 0, MeshletBuffer.Size}};

			vkCmdPushDescriptorSetWithTemplateKHR(CommandBuffer, RtxProgram.DescriptorTemplate, RtxProgram.Layout, 0, DescriptorInfo);

			vkCmdPushConstants(CommandBuffer, RtxProgram.Layout, RtxProgram.Stages, 0, sizeof(globals), &Globals);
			vkCmdDrawMeshTasksIndirectCountNV(CommandBuffer, DrawCommandBuffer.Handle, offsetof(mesh_draw_command, MeshletDrawCommand), DrawCommandCountBuffer.Handle, 0, DrawCount, sizeof(mesh_draw_command));
		}
		else
		{
			vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, MeshPipeline);
			descriptor_template DescriptorInfo[] = {{DrawBuffer.Handle, 0, DrawBuffer.Size},
												    {DrawCommandBuffer.Handle, 0, DrawCommandBuffer.Size},
													{VertexBuffer.Handle, 0, VertexBuffer.Size}};

			vkCmdPushDescriptorSetWithTemplateKHR(CommandBuffer, MeshProgram.DescriptorTemplate, MeshProgram.Layout, 0, DescriptorInfo);
			vkCmdBindIndexBuffer(CommandBuffer, IndexBuffer.Handle, 0, VK_INDEX_TYPE_UINT32);

			vkCmdPushConstants(CommandBuffer, MeshProgram.Layout, MeshProgram.Stages, 0, sizeof(globals), &Globals);
			vkCmdDrawIndexedIndirectCountKHR(CommandBuffer, DrawCommandBuffer.Handle, offsetof(mesh_draw_command, DrawCommand), DrawCommandCountBuffer.Handle, 0, DrawCount, sizeof(mesh_draw_command));
		}

		vkCmdEndRenderPass(CommandBuffer);

		{
			std::vector<VkImageMemoryBarrier> DepthReadBarriers = 
			{
				CreateImageBarrier(DepthTarget.Handle, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT),
				CreateImageBarrier(DepthPyramid.Handle, 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL),
			};
			ImageBarrier(CommandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, DepthReadBarriers);

			vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, DepthReducePipeline);

			for(u32 PyramidIndex = 0;
				PyramidIndex < DepthPyramidLevels;
				++PyramidIndex)
			{
				descriptor_template SourceDepth = (PyramidIndex == 0) ? 
					descriptor_template(DepthSampler, DepthTarget.View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) : 
					descriptor_template(DepthSampler, DepthPyramidMips[PyramidIndex - 1], VK_IMAGE_LAYOUT_GENERAL);
				descriptor_template ComputeDescriptors[] = {SourceDepth, {DepthPyramidMips[PyramidIndex], VK_IMAGE_LAYOUT_GENERAL}};
				vkCmdPushDescriptorSetWithTemplateKHR(CommandBuffer, DepthReduceProgram.DescriptorTemplate, DepthReduceProgram.Layout, 0, ComputeDescriptors);

				u32 LevelWidth  = max(1u, (Swapchain.Width / 2) >> PyramidIndex);
				u32 LevelHeight = max(1u, (Swapchain.Height / 2) >> PyramidIndex);

				depth_reduce_data DepthReduceData = {};
				DepthReduceData.Dims = glm::vec2(LevelWidth, LevelHeight);

				vkCmdPushConstants(CommandBuffer, DepthReduceProgram.Layout, DepthReduceProgram.Stages, 0, sizeof(depth_reduce_data), &DepthReduceData);
				vkCmdDispatch(CommandBuffer, GetGroupCount(LevelWidth, DepthReduceComputeShader.LocalSizeX), GetGroupCount(LevelHeight, DepthReduceComputeShader.LocalSizeY), 1);

				VkImageMemoryBarrier ReduceBarrier = CreateImageBarrier(DepthPyramid.Handle, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
				vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 1, &ReduceBarrier);
			}

			std::vector<VkImageMemoryBarrier> DepthWriteBarriers = 
			{
				CreateImageBarrier(DepthTarget.Handle, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT),
			};
			ImageBarrier(CommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, DepthWriteBarriers);

			VkRenderPassBeginInfo LateRenderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
			LateRenderPassBeginInfo.framebuffer = TargetFramebuffer;
			LateRenderPassBeginInfo.renderPass = RenderPassLate;
			LateRenderPassBeginInfo.renderArea.extent.width = Swapchain.Width;
			LateRenderPassBeginInfo.renderArea.extent.height = Swapchain.Height;

			vkCmdBeginRenderPass(CommandBuffer, &LateRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdEndRenderPass(CommandBuffer);
		}

		vkCmdEndQuery(CommandBuffer, PipelineQueryPool, 0);

		std::vector<VkImageMemoryBarrier> ImageCopyBarriers = 
		{
			CreateImageBarrier(ColorTarget.Handle, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL),
			CreateImageBarrier(Swapchain.Images[ImageIndex], 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
		};

		ImageBarrier(CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, ImageCopyBarriers);

		if(IsPyramidVisualized)
		{
			VkImageBlit BlitRegion = {};
			BlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			BlitRegion.srcSubresource.mipLevel = VisualizedPyramidLevel;
			BlitRegion.srcSubresource.layerCount = 1;
			BlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			BlitRegion.dstSubresource.layerCount = 1;
			BlitRegion.srcOffsets[0] = {0, 0, 0};
			BlitRegion.srcOffsets[1] = {s32(max(1, (Swapchain.Width / 2) >> VisualizedPyramidLevel)), s32(max(1, (Swapchain.Height / 2) >> VisualizedPyramidLevel)), 1};
			BlitRegion.dstOffsets[0] = {0, 0, 0};
			BlitRegion.dstOffsets[1] = {s32(Swapchain.Width), s32(Swapchain.Height), 1};

			vkCmdBlitImage(CommandBuffer, DepthPyramid.Handle, VK_IMAGE_LAYOUT_GENERAL, Swapchain.Images[ImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &BlitRegion, VK_FILTER_NEAREST);
		}
		else
		{
			VkImageCopy ImageCopyRegion = {};
			ImageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			ImageCopyRegion.srcSubresource.layerCount = 1;
			ImageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			ImageCopyRegion.dstSubresource.layerCount = 1;
			ImageCopyRegion.extent = {Swapchain.Width, Swapchain.Height, 1};
			vkCmdCopyImage(CommandBuffer, ColorTarget.Handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Swapchain.Images[ImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &ImageCopyRegion);
		}

		vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampsQueryPool, 1);

		std::vector<VkImageMemoryBarrier> ImagePresentBarrier = 
		{
			CreateImageBarrier(Swapchain.Images[ImageIndex], VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR),
		};
		ImageBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, ImagePresentBarrier);

		vkEndCommandBuffer(CommandBuffer);

		VkPipelineStageFlags SubmitStageFlag = VK_PIPELINE_STAGE_TRANSFER_BIT;
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

		u64 TimeResults[2] = {};
		vkGetQueryPoolResults(Device, TimestampsQueryPool, 0, ArraySize(TimeResults), sizeof(TimeResults), TimeResults, sizeof(TimeResults[0]), VK_QUERY_RESULT_64_BIT);

		u64 PipelineResults[1] = {};
		vkGetQueryPoolResults(Device, PipelineQueryPool, 0, 1, sizeof(PipelineResults), PipelineResults, sizeof(PipelineResults[0]), 0);

		TriangleCount = PipelineResults[0];

		QueryPerformanceCounter(&EndTime);

		CpuAvgTime = CpuAvgTime * 0.75f + ((float)(EndTime.QuadPart - BegTime.QuadPart) / (float)TimeFreq.QuadPart * 1000.0f) * 0.25f;
		GpuAvgTime = GpuAvgTime * 0.75f + ((float)(TimeResults[1] - TimeResults[0]) * (float)Props.limits.timestampPeriod * 1.e-6f) * 0.25f;

		char Title[256];
		sprintf(Title, "%s; %s; %s; Vulkan Engine - cpu: %.2f ms, gpu: %.2f ms; %0.2f cpu FPS; %0.2f gpu FPS; %llu triangles; %llu meshlets", 
				IsRtxEnabled ? "RTX Is Enabled" : "RTX Is Disabled",
				IsCullEnabled == 1 ? "Cull Is Enabled" : "Cull Is Disabled",
				IsLodEnabled == 1 ? "Lod Is Enabled" : "Lod Is Disabled",
				CpuAvgTime, 
			    GpuAvgTime,
				1.0f / (CpuAvgTime) * 1000.0f,
				1.0f / (GpuAvgTime) * 1000.0f,
				TriangleCount,
				Geometries.Meshlets.size());
		SetWindowTextA(Window, Title);
	}

	for(u32 DepthPyramidIdx = 0;
		DepthPyramidIdx < DepthPyramidLevels;
		++DepthPyramidIdx)
	{
		vkDestroyImageView(Device, DepthPyramidMips[DepthPyramidIdx], 0);
	}

	vkDestroyFramebuffer(Device, TargetFramebuffer, 0);
	DestroyImage(DepthPyramid, Device);
	DestroyImage(ColorTarget, Device);
	DestroyImage(DepthTarget, Device);

	if(IsRtxSupported)
	{
		DestroyBuffer(MeshletBuffer, Device);
		DestroyBuffer(MeshletDataBuffer, Device);
	}

	DestroyBuffer(MeshBuffer, Device);
	DestroyBuffer(DrawCommandCountBuffer, Device);
	DestroyBuffer(DrawBuffer, Device);
	DestroyBuffer(DrawCommandBuffer, Device);
	DestroyBuffer(VertexBuffer, Device);
	DestroyBuffer(IndexBuffer, Device);
	DestroyBuffer(ScratchBuffer, Device);

	vkDestroyQueryPool(Device, TimestampsQueryPool, 0);
	vkDestroyQueryPool(Device, PipelineQueryPool, 0);

	vkDestroyCommandPool(Device, CommandPool, 0);

	vkDestroySampler(Device, DepthSampler, 0);

	DestroySwapchain(Swapchain, Device);

	vkDestroyRenderPass(Device, RenderPass, 0);
	vkDestroyRenderPass(Device, RenderPassLate, 0);

	vkDestroyPipeline(Device, DepthReducePipeline, 0);
	DeleteProgram(DepthReduceProgram, Device);

	vkDestroyPipeline(Device, DrawCmdPipeline, 0);
	DeleteProgram(ComputeProgram, Device);

	vkDestroyPipeline(Device, MeshPipeline, 0);
	DeleteProgram(MeshProgram, Device);

	if(IsRtxSupported)
	{
		vkDestroyPipeline(Device, RtxPipeline, 0);
		DeleteProgram(RtxProgram, Device);

		vkDestroyShaderModule(Device, ObjectMeshShader.Handle, 0);
		vkDestroyShaderModule(Device, ObjectTaskShader.Handle, 0);
	}

	vkDestroyShaderModule(Device, DepthReduceComputeShader.Handle, 0);
	vkDestroyShaderModule(Device, CommandComputeShader.Handle, 0);
	vkDestroyShaderModule(Device, ObjectFragmentShader.Handle, 0);
	vkDestroyShaderModule(Device, ObjectVertexShader.Handle, 0);

	vkDestroySemaphore(Device, AcquireSemaphore, 0);
	vkDestroySemaphore(Device, ReleaseSemaphore, 0);

	vkDestroySurfaceKHR(Instance, Surface, 0);
	vkDestroyDevice(Device, 0);
	vkDestroyDebugReportCallbackEXT(Instance, Callback, 0);
	vkDestroyInstance(Instance, 0);

	return 0;
}

LRESULT CALLBACK 
WindowProc(HWND Wnd, UINT Msg, WPARAM wParam, LPARAM lParam)
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
				bool IsLeftShift = KeyCode & VK_LSHIFT;

				if(IsDown)
				{
					if(KeyCode == 'R')
					{
						IsRtxEnabled = !IsRtxEnabled;
					}
					if(KeyCode == 'L')
					{
						IsLodEnabled = !IsLodEnabled;
					}
					if(KeyCode == 'C')
					{
						IsCullEnabled = !IsCullEnabled;
					}
					if(KeyCode == 'P')
					{
						if(IsPyramidVisualized)
						{
							LastVisualizedPyramidLevel = VisualizedPyramidLevel;
							VisualizedPyramidLevel = (VisualizedPyramidLevel + 1) % GlobalDepthPyramidLevels;
							if(LastVisualizedPyramidLevel == (GlobalDepthPyramidLevels - 1))
							{
								IsPyramidVisualized = !IsPyramidVisualized;
							}
						}
						else
						{
							IsPyramidVisualized = !IsPyramidVisualized;
						}
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

// NOTE: Only when console output is needed
// Otherwise it is directly call WinMain
int main(int argc, char* argv[])
{
	WinMain(NULL, NULL, argv[0], SW_SHOWNORMAL);
}

