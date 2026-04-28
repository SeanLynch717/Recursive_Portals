#include "ShaderIncludes.hlsli"


cbuffer ExternalData: register(b0)
{
	float3 cameraPosition;
	float roughness;
	float3 ambientColor;
	Light lights[5];
	int drawRecursive;
	float scale;
	float3 borderColor;
	float borderThickness;
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
	float upperBound = sin(scale * PI / 2) * 0.5;
	float lowerBound = sin(scale * PI / 2) * 0.5 - borderThickness;
	// Draw border
	if (squareRoot < lowerBound) {
		return float4(0, 0, 0, drawRecursive ? 0 : 1);
	}
	else if (squareRoot < upperBound) {
		return float4(borderColor, 1);
	} else {
		discard;
	}
	return float4(0, 0, 0, 0);
}