#include "ShaderIncludes.hlsli"

cbuffer ExternalData: register(b0)
{
	float4 colorTint;
	float3 cameraPosition;
	float roughness;
	float3 ambientColor;
	int hasSpecular;
	int hasNormal;
	Light lights[5];
}

// Texture related resources
Texture2D Albedo			: register(t0); // Textures use "t" registers
Texture2D NormalMap			: register(t1);
Texture2D RoughnessMap		: register(t2);
Texture2D MetalnessMap		: register(t3);

SamplerState BasicSampler	: register(s0); // Samplers use "s" registers

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
	// Always re-normalize any interpolated normals
	input.normal = normalize(input.normal);
	
	// Adjust the normal if this object was provided a normal map.
	if (hasNormal) {
		float3 unpackedNormal = NormalMap.Sample(BasicSampler, input.uv).rgb * 2 - 1;
		input.tangent = normalize(input.tangent - input.normal * dot(input.tangent, input.normal));
		float3 B = cross(input.tangent, input.normal);
		float3x3 TBN = float3x3(input.tangent, B, input.normal);
		input.normal = mul(unpackedNormal, TBN);
	}

	// Get the roughness of the texture at this location
	float roughness = hasSpecular == 1 ? RoughnessMap.Sample(BasicSampler, input.uv).r : 1.0f;

	// Get the metalness of the texture.
	float metalness = MetalnessMap.Sample(BasicSampler, input.uv).r;

	// Sample the texture and tint for the final surface color
	float3 surfaceColor = pow(Albedo.Sample(BasicSampler, input.uv).rgb, 2.2f);

	// Specular color differes between metal surfaces.
	float3 specularColor = lerp(F0_NON_METAL.rrr, surfaceColor.rgb, metalness);

	float3 totalLight = ambientColor * surfaceColor.rgb;

	// Calculate the emmissive light for each light source.
	for (int i = 0; i < 5; i++) {
		Light light = lights[i];
		light.Direction = normalize(light.Direction);
		switch (light.Type) {
			// Directional light.
			case 0: {
				totalLight += DirLightPBR(light, input.normal, input.worldPosition, cameraPosition, roughness, metalness, surfaceColor.rgb, specularColor);
				break;
			}
			// Point light.
			case 1: {
				totalLight += PointLightPBR(light, input.normal, input.worldPosition, cameraPosition, roughness, metalness, surfaceColor.rgb, specularColor);
				break;
			}
		}
	}
	return float4(pow(totalLight, 1.0f / 2.2f), 1);
}