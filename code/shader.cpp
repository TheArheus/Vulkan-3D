
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
		if(Var.IsValid && Var.StorageClass == SpvStorageClassStorageBuffer)
		{
			Shader.StorageBufferMask |= 1 << Var.Binding;
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
CreateDescriptorSetLayout(VkDevice Device, shader& VS, shader& FS)
{
	VkDescriptorSetLayout Result = 0;

	std::vector<VkDescriptorSetLayoutBinding> SetBindings;

	u32 StorageBufferMask = VS.StorageBufferMask | FS.StorageBufferMask;

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
			if(VS.StorageBufferMask & (1 << BitIndex))
			{
				NewBinding.stageFlags |= VS.Stage;
			}
			if(FS.StorageBufferMask & (1 << BitIndex))
			{
				NewBinding.stageFlags |= FS.Stage;
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
CreatePipelineLayout(VkDevice Device, VkDescriptorSetLayout DescriptorSetLayout)
{
	VkPipelineLayout Result = {};

	VkPipelineLayoutCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	CreateInfo.pSetLayouts = &DescriptorSetLayout;
	CreateInfo.setLayoutCount = 1;

	VK_CHECK(vkCreatePipelineLayout(Device, &CreateInfo, 0, &Result));

	return Result;
}

internal VkDescriptorUpdateTemplate
CreateDescriptorTemplate(VkDevice Device, VkPipelineBindPoint BindPoint, VkPipelineLayout PipelineLayout, shader& VS, shader& FS)
{
	VkDescriptorUpdateTemplate Result = 0;
	std::vector<VkDescriptorUpdateTemplateEntry> Entries;

	u32 StorageBufferMask = VS.StorageBufferMask | FS.StorageBufferMask;

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
CreateGraphicsPipeline(VkDevice Device, VkPipelineCache PipelineCache, VkPipelineLayout Layout, VkRenderPass RenderPass, const shader& VS, const shader& FS)
{
	VkGraphicsPipelineCreateInfo CreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

	VkPipelineShaderStageCreateInfo Stages[2] = {};
	Stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	Stages[0].module = VS.Handle;
	Stages[0].stage = VS.Stage;
	Stages[0].pName = "main";
	Stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	Stages[1].module = FS.Handle;
	Stages[1].stage = FS.Stage;
	Stages[1].pName = "main";

	CreateInfo.layout = Layout;
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
