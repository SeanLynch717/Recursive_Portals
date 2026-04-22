#include "ShaderIncludes.hlsli"


cbuffer ExternalData: register(b0)
{
	float3 cameraPosition;
	float roughness;
	float3 ambientColor;
	Light lights[5];
	int drawRecursive;
	float totalTime;
	float3 borderColor;
}

// --------------------------------------------------------
// The entry point (main method) for our pixel shader
// 
// - Input is the data coming down the pipeline (defined by the struct)
// - Output is a single color (float4)
// - Has a special semantic (SV_TARGET), which means 
//    "put the output of this into the current render target"
// - Named "main" because that's the default the shader compiler looks for
// --------------------------------------------------------
float4 main(VertexToPixel input) : SV_TARGET
{
	float squareRoot = sqrt(pow(input.uv.x - 0.5f, 2) + pow(input.uv.y - 0.5f, 2));
	float lowerBound = abs(cos(totalTime * 2) / 2.3);
	// Draw border
	if (squareRoot > 0.45f) {
		return float4(borderColor, 1);
	}
	return float4(0, 0, 0, drawRecursive ? 0 : 1);
}