#version 450

struct depth_reduce_data
{
	vec2 Dims;
};

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) uniform sampler2D Input;
layout(binding = 1, r32f) uniform writeonly image2D Output;
layout(push_constant) uniform block
{
	depth_reduce_data DepthReduceData;
};

void main()
{
	uvec2 Pos = gl_GlobalInvocationID.xy;

	vec4 Depth4 = textureGather(Input, (Pos + vec2(0.5)) / DepthReduceData.Dims);
	float Depth = min(min(Depth4.x, Depth4.y), min(Depth4.z, Depth4.w));
	imageStore(Output, ivec2(Pos), vec4(Depth));
}

