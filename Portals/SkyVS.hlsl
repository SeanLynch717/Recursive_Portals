#include "ShaderIncludes.hlsli"

// Constant buffer used to allocate a chunk of memory on the GPU side accessable by shaders.
cbuffer ExternalData : register(b0)
{
	matrix view;
	matrix projection;
}

VertexToPixel_Sky main( VertexShaderInput input )
{
	VertexToPixel_Sky output;

	// Get rid of translation.
	matrix viewNoTranslation = view;
	viewNoTranslation._14 = 0;
	viewNoTranslation._24 = 0;
	viewNoTranslation._34 = 0;

	// Multiply the view with no translation by the projection matrix and apply the transformation to the vertices position.
	matrix vp = mul(projection, viewNoTranslation);
	output.position = mul(vp, float4(input.position, 1));

	// Force the z-depth to be 1. We set it to w to account for the w divide 
	// at the next stage of the pipeline since w/w = 1, given w != 0.
	output.position.z = output.position.w;

	// The direction we are sampling the cube map is the same as the vertices position since the center of the cube is at the origin.
	output.sampleDir = input.position;

	return output;
}