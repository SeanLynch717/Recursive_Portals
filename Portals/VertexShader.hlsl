#include "ShaderIncludes.hlsli"

// Constant buffer used to allocate a chunk of memory on the GPU side accessable by shaders.
cbuffer ExternalData : register(b0) 
{ 
	matrix world; 
	matrix view;
	matrix projection;
	matrix worldInverseTranspose;
}

// --------------------------------------------------------
// The entry point (main method) for our vertex shader
// 
// - Input is exactly one vertex worth of data (defined by a struct)
// - Output is a single struct of data to pass down the pipeline
// - Named "main" because that's the default the shader compiler looks for
// --------------------------------------------------------
VertexToPixel main( VertexShaderInput input )
{
	// Set up output struct
	VertexToPixel output;

	// Screen position of vertex
	matrix wvp = mul(projection, mul(view, world));
	output.position = mul(wvp, float4(input.position, 1.0f));
	output.normal = mul((float3x3)worldInverseTranspose, input.normal); // mul by inverse transpose
	output.tangent = mul((float3x3)worldInverseTranspose, input.tangent); // again, multiply by inverse transpose
	output.uv = input.uv;
	output.screenPosition = input.position;
	output.worldPosition = mul(world, float4(input.position, 1)).xyz;

	// Whatever we return will make its way through the pipeline to the
	// next programmable stage we're using (the pixel shader for now)
	return output;
}