
struct depth_reduce_data
{
	float2 Dims;
};

[[vk::combinedImageSampler]][[vk::binding(0)]]
Texture2D<float> InputTexture;
[[vk::combinedImageSampler]][[vk::binding(0)]]
SamplerState InputTextureSampler;

[[vk::binding(1)]] RWTexture2D<float> Output;
[[vk::push_constant]] depth_reduce_data DepthReduceData;

[numthreads(32, 32, 1)]
void main(uint3 GlobalInvocationID : SV_DispatchThreadID)
{
	uint2 Pos = GlobalInvocationID.xy;

	float Depth = InputTexture.Gather(InputTextureSampler, (Pos + float2(0.5, 0.5)) / DepthReduceData.Dims).x;

	Output[Pos] = Depth;
}

