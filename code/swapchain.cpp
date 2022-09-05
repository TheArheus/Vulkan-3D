
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
