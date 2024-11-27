
internal VkFramebuffer
CreateFramebuffer(VkDevice Device, VkRenderPass RenderPass, VkImageView ColorView, VkImageView DepthView, u32 Width, u32 Height)
{
	VkImageView AttachmentViews[] = {ColorView, DepthView};

	VkFramebufferCreateInfo FramebufferCreateInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	FramebufferCreateInfo.pAttachments = AttachmentViews;
	FramebufferCreateInfo.attachmentCount = 2;
	FramebufferCreateInfo.width = Width;
	FramebufferCreateInfo.height = Height;
	FramebufferCreateInfo.renderPass = RenderPass;
	FramebufferCreateInfo.layers = 1;
	VkFramebuffer Result = 0;
	vkCreateFramebuffer(Device, &FramebufferCreateInfo, 0, &Result);
	return Result;
}

internal VkImageView
CreateImageView(VkDevice Device, VkImage Image, VkFormat Format, u32 MipLevel, u32 LevelCount)
{
	VkImageAspectFlags Aspect = (Format == VK_FORMAT_D32_SFLOAT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	VkImageViewCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	CreateInfo.format = Format;
	CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	CreateInfo.image = Image;
	CreateInfo.subresourceRange.aspectMask = Aspect;
	CreateInfo.subresourceRange.baseMipLevel = MipLevel;
	CreateInfo.subresourceRange.layerCount = 1;
	CreateInfo.subresourceRange.levelCount = LevelCount;

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
	SwapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
}

internal void
DestroySwapchain(const swapchain& Swapchain, VkDevice Device)
{
	vkDestroySwapchainKHR(Device, Swapchain.Handle, 0);
}

internal bool
ResizeSwapchain(swapchain& Result, VkPhysicalDevice PhysicalDevice, VkRenderPass RenderPass, VkDevice Device, VkSurfaceKHR Surface, VkSurfaceFormatKHR SurfaceFormat, VkSurfaceCapabilitiesKHR SurfaceCaps, u32* FamilyIndex)
{
	VkSurfaceCapabilitiesKHR ResizeCaps;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &ResizeCaps));
	u32 NewWidth = ResizeCaps.currentExtent.width;
	u32 NewHeight = ResizeCaps.currentExtent.height;
	if(NewWidth == Result.Width && NewHeight == Result.Height)
	{
		return false;
	}

	VkSwapchainKHR OldSwapchain = Result.Handle;
	swapchain Old = Result;
	CreateSwapchain(Result, RenderPass, Device, Surface, SurfaceFormat, SurfaceCaps, NewWidth, NewHeight, FamilyIndex, OldSwapchain);
	VK_CHECK(vkDeviceWaitIdle(Device));
	DestroySwapchain(Old, Device);

	return true;
}
