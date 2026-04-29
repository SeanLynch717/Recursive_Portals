#include "ShaderIncludes.hlsli"


cbuffer ExternalData: register(b0)
{
	float3 cameraPosition;
	float roughness;
	float3 ambientColor;
	Light lights[5];
	int drawRecursive;
	int recursionLevel;
	float scale;
	float3 borderColor;
	float borderThickness;
	float totalTime;
	float portalRippleStrength;
}

Texture2D SceneCapture : register(t0);

SamplerState BasicSampler : register(s0);

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
		if (drawRecursive == 0) {
			return float4(0, 0, 0, 1);
		}
		// Capture the dimensions of the copy of the back buffer
		uint width, height;
		// Convert screen space to normalized device coordinates [0, 1]
		SceneCapture.GetDimensions(width, height);
		float2 screenUV = input.position.xy / float2(width, height);
		if (recursionLevel == 0 && portalRippleStrength > 0) {
			// Portal surface UVs usually range [0, 1]. 
			// Calculate the vector from the center (0.5, 0.5) to the current pixel.
			float2 toCenter = input.uv - float2(0.5f, 0.5f);
			
			// Calculate the distance from the center (radius)
			float dist = length(toCenter);
			
			// Ripple Parameters
			float rippleStrength = 0.01f * EaseOutQuad(portalRippleStrength); 
			float freq = 20.0f;
			float speed = 10.0f;
			
			// Using minus for speed makes the waves move OUTWARD.
			float wave = sin((dist * freq) - (totalTime * speed));
			
			// Masking: We want the ripple to fade out at the edges and be 0 in the very center
			// This prevents the "pinched" look at the origin.
			float mask = saturate(dist * 2.0f); // Simple linear ramp
			float distortion = wave * rippleStrength * mask;
			
			// Sample the scene
			// We move the screenUV in the direction of 'toCenter' to create a refraction look
			screenUV = screenUV + (normalize(toCenter) * distortion);
		}
		
		float3 sceneColor = SceneCapture.Sample(BasicSampler, screenUV).rgb;
		
		return float4(sceneColor, 1.0f);
	}
	else if (squareRoot < upperBound) {
		return float4(borderColor, 1);
	} else {
		discard;
	}
	return float4(0, 0, 0, 0);
}