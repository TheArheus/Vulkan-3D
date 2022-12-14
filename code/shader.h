
struct shader
{
	VkShaderModule Handle;
	VkShaderStageFlagBits Stage;

	VkDescriptorType ResourceTypes[32];
	u32 ResourceMask;

	u32 LocalSizeX;
	u32 LocalSizeY;
	u32 LocalSizeZ;
	bool IsUsingPushConstant;
};

struct program
{
	VkPipelineLayout Layout;
	VkDescriptorSetLayout DescriptorSetLayout;
	VkDescriptorUpdateTemplate DescriptorTemplate;
	VkShaderStageFlags Stages;
};

using shaders = std::initializer_list<const shader*>;

struct descriptor_template
{
	union
	{
		VkDescriptorImageInfo Image;
		VkDescriptorBufferInfo Buffer;
	};

	descriptor_template(VkBuffer BufferHandle, VkDeviceSize Offset, VkDeviceSize Size)
	{
		Buffer.buffer = BufferHandle;
		Buffer.offset = Offset;
		Buffer.range = Size;
	}
	
	descriptor_template(VkBuffer BufferHandle)
	{
		Buffer.buffer = BufferHandle;
		Buffer.offset = 0;
		Buffer.range = VK_WHOLE_SIZE;
	}

	descriptor_template(VkSampler Sampler, VkImageView ImageView, VkImageLayout ImageLayout)
	{
		Image.sampler = Sampler;
		Image.imageView = ImageView;
		Image.imageLayout = ImageLayout;
	}

	descriptor_template(VkImageView ImageView, VkImageLayout ImageLayout)
	{
		Image.sampler = VK_NULL_HANDLE;
		Image.imageView = ImageView;
		Image.imageLayout = ImageLayout;
	}
};
