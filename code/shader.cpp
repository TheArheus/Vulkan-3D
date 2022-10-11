
struct resource
{
	bool IsValid;
	u32  Type;
	u32  StorageClass;
	u32  DescriptorSet;
	u32  Binding;
};

internal VkShaderStageFlagBits
GetShaderStage(SpvExecutionModel ExecutionModel)
{
	switch(ExecutionModel)
	{
		case SpvExecutionModelGLCompute:
		{
			return VK_SHADER_STAGE_COMPUTE_BIT;
		} break;
		case SpvExecutionModelVertex:
		{
			return VK_SHADER_STAGE_VERTEX_BIT;
		} break;
		case SpvExecutionModelFragment:
		{
			return VK_SHADER_STAGE_FRAGMENT_BIT;
		} break;
		case SpvExecutionModelMeshNV:
		{
			return VK_SHADER_STAGE_MESH_BIT_NV;
		} break;
		case SpvExecutionModelTaskNV:
		{
			return VK_SHADER_STAGE_TASK_BIT_NV;
		} break;
		default:
		{
			return VkShaderStageFlagBits(0);
		}break;
	}
}

internal void
ParseShader(shader& Shader, const u32* Code, u32 CodeSize)
{
	assert(Code[0] == SpvMagicNumber);

	u32 IdBound = Code[3];
	const u32* Instruction = Code + 5;

	std::vector<resource> Vars(IdBound);

	while(Instruction != (Code + CodeSize))
	{
		u16 OpCode = (u16)(Instruction[0]);
		u16 WordCount = (u16)(Instruction[0] >> 16);

		switch(OpCode)
		{
			case SpvOpEntryPoint:
			{
				assert(WordCount >= 2);
				Shader.Stage = GetShaderStage((SpvExecutionModel)Instruction[1]);
			} break;
			case SpvOpDecorate:
			{
				assert(WordCount >= 3);

				u32 Id = Instruction[1];
				u32 DecInd = Instruction[2];
				u32 DecVal = Instruction[3];

				switch(DecInd)
				{
					case SpvDecorationDescriptorSet:
					{
						Vars[Id].DescriptorSet = DecVal;
					} break;
					case SpvDecorationBinding:
					{
						Vars[Id].Binding = DecVal;
					} break;
				}
			} break;
			case SpvOpVariable:
			{
				u32 Id = Instruction[2];
				u32 Type = Instruction[1];
				u32 StorageClass = Instruction[3];

				Vars[Id].IsValid = true;
				Vars[Id].Type = Type;
				Vars[Id].StorageClass = StorageClass;
			} break;
		}

		assert((Instruction + WordCount) <= (Code + CodeSize));
		Instruction += WordCount;
	}

	for(auto Var : Vars)
	{
		if(Var.IsValid && (Var.StorageClass == SpvStorageClassStorageBuffer || Var.StorageClass == SpvStorageClassUniformConstant))
		{
			assert(Var.DescriptorSet == 0);
			assert(Var.Binding < 32);
			Shader.StorageBufferMask |= 1 << Var.Binding;
		}

		if(Var.IsValid && (Var.StorageClass == SpvStorageClassPushConstant))
		{
			Shader.IsUsingPushConstant = true;
		}
	}
}

internal bool
LoadShader(shader& Shader, VkDevice Device, const char* Path)
{
	bool Result = false;
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

		ParseShader(Shader, reinterpret_cast<const u32*>(Buffer), FileLength / 4);

		VkShaderModuleCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
		CreateInfo.codeSize = FileLength;
		CreateInfo.pCode = reinterpret_cast<const u32*>(Buffer);

		VK_CHECK(vkCreateShaderModule(Device, &CreateInfo, 0, &Shader.Handle));
		Result = true;

		free(Buffer);
	}
	fclose(File);
	return Result;
}

internal VkDescriptorSetLayout
CreateDescriptorSetLayout(VkDevice Device, shaders Shaders)
{
	VkDescriptorSetLayout Result = 0;

	std::vector<VkDescriptorSetLayoutBinding> SetBindings;

	u32 StorageBufferMask = 0;
	for(const shader* Shader : Shaders)
	{
		StorageBufferMask |= Shader->StorageBufferMask;
	}

	for(u32 BitIndex = 0;
		BitIndex < 32;
		++BitIndex)
	{
		if(StorageBufferMask & (1 << BitIndex))
		{
			VkDescriptorSetLayoutBinding NewBinding = {};
			NewBinding.binding = BitIndex;
			NewBinding.descriptorCount = 1;
			NewBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			NewBinding.stageFlags = 0;

			for(const shader* Shader : Shaders)
			{
				if(Shader->StorageBufferMask & (1 << BitIndex))
				{
					NewBinding.stageFlags |= Shader->Stage;
				}
			}
			SetBindings.push_back(NewBinding);
		}
	}

	VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO}; 
	DescriptorSetLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	DescriptorSetLayoutCreateInfo.bindingCount = (u32)SetBindings.size();
	DescriptorSetLayoutCreateInfo.pBindings = SetBindings.data();

	VK_CHECK(vkCreateDescriptorSetLayout(Device, &DescriptorSetLayoutCreateInfo, 0, &Result));

	return Result;
}

internal VkPipelineLayout
CreatePipelineLayout(VkDevice Device, VkDescriptorSetLayout DescriptorSetLayout, VkShaderStageFlags PushConstantStages, shaders Shaders, size_t PushConstantSize)
{
	VkPipelineLayout Result = {};

	VkPipelineLayoutCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	CreateInfo.pSetLayouts = &DescriptorSetLayout;
	CreateInfo.setLayoutCount = 1;

	VkPushConstantRange ConstantRange = {};
	if(PushConstantSize)
	{
		ConstantRange.stageFlags = PushConstantStages;
		ConstantRange.size = (u32)PushConstantSize;
		CreateInfo.pushConstantRangeCount = 1;
		CreateInfo.pPushConstantRanges = &ConstantRange;
	}

	VK_CHECK(vkCreatePipelineLayout(Device, &CreateInfo, 0, &Result));

	return Result;
}

internal VkDescriptorUpdateTemplate
CreateDescriptorTemplate(VkDevice Device, VkPipelineBindPoint BindPoint, VkPipelineLayout PipelineLayout, shaders Shaders)
{
	VkDescriptorUpdateTemplate Result = 0;
	std::vector<VkDescriptorUpdateTemplateEntry> Entries;

	u32 StorageBufferMask = 0;
	for(const shader* Shader : Shaders)
	{
		StorageBufferMask |= Shader->StorageBufferMask;
	}

	for(u32 BitIndex = 0;
		BitIndex < 32;
		++BitIndex)
	{
		if(StorageBufferMask & (1 << BitIndex))
		{
			VkDescriptorUpdateTemplateEntry DescriptorSet = {};

			DescriptorSet.dstBinding = BitIndex;
			DescriptorSet.dstArrayElement = 0;
			DescriptorSet.descriptorCount = 1;
			DescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			DescriptorSet.offset = sizeof(descriptor_template) * BitIndex;
			DescriptorSet.stride = sizeof(descriptor_template);

			Entries.push_back(DescriptorSet);
		}
	}

	VkDescriptorUpdateTemplateCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO};
	CreateInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
	CreateInfo.descriptorUpdateEntryCount = (u32)Entries.size();
	CreateInfo.pDescriptorUpdateEntries = Entries.data();
	CreateInfo.pipelineBindPoint = BindPoint;
	CreateInfo.pipelineLayout = PipelineLayout;

	VK_CHECK(vkCreateDescriptorUpdateTemplate(Device, &CreateInfo, 0, &Result));

	return Result;
}

internal VkPipeline
CreateGraphicsPipeline(VkDevice Device, VkPipelineCache PipelineCache, VkPipelineLayout Layout, VkRenderPass RenderPass, shaders Shaders)
{
	VkGraphicsPipelineCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

	std::vector<VkPipelineShaderStageCreateInfo> Stages;
	for(const shader* Shader : Shaders)
	{
		VkPipelineShaderStageCreateInfo Stage = {};

		Stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		Stage.module = Shader->Handle;
		Stage.stage = Shader->Stage;
		Stage.pName = "main";

		Stages.push_back(Stage);
	}

	CreateInfo.layout = Layout;
	CreateInfo.renderPass = RenderPass;
	CreateInfo.pStages = Stages.data();
	CreateInfo.stageCount = (u32)Stages.size();

	VkPipelineVertexInputStateCreateInfo VertexInputState = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

	VkPipelineInputAssemblyStateCreateInfo InputAssemblyState = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	InputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineColorBlendStateCreateInfo ColorBlendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	VkPipelineColorBlendAttachmentState ColorAttachmentState = {};
	ColorAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	ColorBlendState.pAttachments = &ColorAttachmentState;
	ColorBlendState.attachmentCount = 1;

	VkPipelineDepthStencilStateCreateInfo DepthStencilState = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
	DepthStencilState.depthTestEnable = true;
	DepthStencilState.depthWriteEnable = true;
	DepthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER;

	VkPipelineViewportStateCreateInfo ViewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	ViewportState.viewportCount = 1;
	ViewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo RasterizationState = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	RasterizationState.lineWidth = 1.0f;
	RasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;

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

internal program
CreateProgram(VkDevice Device, VkPipelineBindPoint BindPoint, shaders Shaders, size_t PushConstantSize)
{
	program Result = {};

	for(const shader* Shader : Shaders)
	{
		if(Shader->IsUsingPushConstant)
			Result.Stages |= Shader->Stage;
	}

	Result.DescriptorSetLayout = CreateDescriptorSetLayout(Device, Shaders);
	assert(Result.DescriptorSetLayout);

	Result.Layout = CreatePipelineLayout(Device, Result.DescriptorSetLayout, Result.Stages, Shaders, PushConstantSize);
	assert(Result.Layout);

	Result.DescriptorTemplate = CreateDescriptorTemplate(Device, BindPoint, Result.Layout, Shaders);
	assert(Result.DescriptorTemplate);

	return Result;
}

internal void
DeleteProgram(program& Program, VkDevice Device)
{
	vkDestroyDescriptorSetLayout(Device, Program.DescriptorSetLayout, 0);
	vkDestroyPipelineLayout(Device, Program.Layout, 0);
	vkDestroyDescriptorUpdateTemplate(Device, Program.DescriptorTemplate, 0);
}

internal VkPipeline 
CreateComputePipeline(VkDevice Device, VkPipelineCache PipelineCache, VkPipelineLayout Layout, shader Shader)
{
	assert(Shader.Stage == VK_SHADER_STAGE_COMPUTE_BIT);

	VkPipeline Pipeline = 0;
	VkComputePipelineCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};

	VkPipelineShaderStageCreateInfo Stage = {};

	Stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	Stage.module = Shader.Handle;
	Stage.stage = Shader.Stage;
	Stage.pName = "main";

	CreateInfo.stage = Stage;
	CreateInfo.layout = Layout;
	vkCreateComputePipelines(Device, PipelineCache, 1, &CreateInfo, 0, &Pipeline);

	return Pipeline;
}

