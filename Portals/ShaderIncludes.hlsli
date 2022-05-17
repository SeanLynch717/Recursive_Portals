// Include guard in hlsl headers
#ifndef __GGP_SHADER_INCLUDES__
#define __GGP_SHADER_INCLUDES__

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

#define MAX_SPECULAR_EXPONENT 256.0f								 

// Structs
struct Light
{
	int Type;
	float3 Direction;
	float Range;
	float3 Position;
	float Intensity;
	float3 Color;
	float SpotFalloff;
	float3 Padding;          // Don't technically need padding at the end, unless you want an array of these.
};

// Struct representing a single vertex worth of data
// - This should match the vertex definition in our C++ code
// - By "match", I mean the size, order and number of members
// - The name of the struct itself is unimportant, but should be descriptive
// - Each variable must have a semantic, which defines its usage
struct VertexShaderInput
{
	float3 position		: POSITION;     // XYZ position
	float3 normal		: NORMAL;
	float3 tangent		: TANGENT;
	float2 uv			: TEXCOORD;
};

// Struct representing the data we're sending down the pipeline
// - Should match our pixel shader's input (hence the name: Vertex to Pixel)
// - At a minimum, we need a piece of data defined tagged as SV_POSITION
// - The name of the struct itself is unimportant, but should be descriptive
// - Each variable must have a semantic, which defines its usage
struct VertexToPixel
{
	float4 position			: SV_POSITION;	// XYZW position (System Value Position)
	float3 normal			: NORMAL;
	float3 tangent			: TANGENT;
	float2 uv				: TEXCOORD;
	float3 worldPosition	: POSITION;
	float3 screenPosition	: SCREENPOSITION;
};
struct VertexToPixel_Sky
{
	float4 position		: SV_POSITION;
	float3 sampleDir	: DIRECTION;
};

float3 Diffuse(float3 normal, float3 dirToLight) {
	// diffuse calculation
	// Saturate always clamps between 0 and 1.
	return saturate(dot(normal, dirToLight));
}

float3 Specular(float3 normal, float3 dirToLight, float3 toCamera, float roughness) {
	float3 refl = reflect(-dirToLight, normal);
	// If the roughness is one, don't apply any specular.
	return roughness == 1 ? 0 : pow(max(dot(toCamera, refl), 0), (1 - roughness) * MAX_SPECULAR_EXPONENT);
}

float3 DirectionalLight(Light light, float3 normal, float roughness, float3 cameraPosition, float3 worldPosition, float3 surfaceColor, float specularScale) {
	float3 toLight = normalize(-light.Direction);
	float3 toCam = normalize(cameraPosition - worldPosition);

	float diff = Diffuse(normal, toLight);
	float spec = any(diff) * Specular(normal, toLight, toCam, roughness) * specularScale;

	return (diff * surfaceColor + spec) * light.Intensity * light.Color;
}

float Attenuate(Light light, float3 worldPosition) {
	float dist = distance(light.Position, worldPosition);
	float att = saturate(1.0f - ((dist * dist) / (light.Range * light.Range)));
	return att * att;
}

float3 PointLight(Light light, float3 normal, float roughness, float3 cameraPosition, float3 worldPosition, float3 surfaceColor, float specularScale) {
	float3 toLight = normalize(light.Position - worldPosition);
	float3 toCam = normalize(cameraPosition - worldPosition);

	float atten = Attenuate(light, worldPosition);
	float diff = Diffuse(normal, toLight);
	float spec = any(diff) * Specular(normal, toLight, toCam, roughness) * specularScale;

	return (diff * surfaceColor + spec) * atten * light.Intensity * light.Color;
}

// PHYSICALLY BASED LIGHTING----------------------------------------------------------
static const float F0_NON_METAL = 0.04f;

// Need a minimum roughness for when spec distribution function denominator goes to zero
static const float MIN_ROUGHNESS = 0.0000001f; // 6 zeros after decimal

static const float PI = 3.14159265359f;
static const float TWO_PI = PI * 2.0f;
static const float HALF_PI = PI / 2.0f;
static const float QUARTER_PI = PI / 4.0f;

// Lambert diffuse BRDF - Same as the basic lighting!
float DiffusePBR(float3 normal, float3 dirToLight)
{
	return saturate(dot(normal, dirToLight));
}

// GGX (Trowbridge-Reitz)
//
// a - Roughness
// h - Half vector
// n - Normal
// 
// D(h, n) = a^2 / pi * ((n dot h)^2 * (a^2 - 1) + 1)^2
float SpecDistribution(float3 n, float3 h, float roughness)
{
	// Pre-calculations
	float NdotH = saturate(dot(n, h));
	float NdotH2 = NdotH * NdotH;
	float a = roughness * roughness;
	float a2 = max(a * a, MIN_ROUGHNESS); // Applied after remap!

	// ((n dot h)^2 * (a^2 - 1) + 1)
	float denomToSquare = NdotH2 * (a2 - 1) + 1;
	// Can go to zero if roughness is 0 and NdotH is 1

	// Final value
	return a2 / (PI * denomToSquare * denomToSquare);
}



// Fresnel term - Schlick approx.
// 
// v - View vector
// h - Half vector
// f0 - Value when l = n (full specular color)
//
// F(v,h,f0) = f0 + (1-f0)(1 - (v dot h))^5
float3 Fresnel(float3 v, float3 h, float3 f0)
{
	// Pre-calculations
	float VdotH = saturate(dot(v, h));

	// Final value
	return f0 + (1 - f0) * pow(1 - VdotH, 5);
}



// Geometric Shadowing - Schlick-GGX (based on Schlick-Beckmann)
// - k is remapped to a / 2, roughness remapped to (r+1)/2
//
// n - Normal
// v - View vector
//
// G(l,v,h)
float GeometricShadowing(float3 n, float3 v, float3 h, float roughness)
{
	// End result of remapping:
	float k = pow(roughness + 1, 2) / 8.0f;
	float NdotV = saturate(dot(n, v));

	// Final value
	return NdotV / (NdotV * (1 - k) + k);
}



// Microfacet BRDF (Specular)
//
// f(l,v) = D(h)F(v,h)G(l,v,h) / 4(n dot l)(n dot v)
// - part of the denominator are canceled out by numerator (see below)
//
// D() - Spec Dist - Trowbridge-Reitz (GGX)
// F() - Fresnel - Schlick approx
// G() - Geometric Shadowing - Schlick-GGX
float3 MicrofacetBRDF(float3 n, float3 l, float3 v, float roughness, float3 specColor)
{
	// Other vectors
	float3 h = normalize(v + l);

	// Grab various functions
	float D = SpecDistribution(n, h, roughness);
	float3 F = Fresnel(v, h, specColor); // This ranges from F0_NON_METAL to actual specColor based on metalness
	float G = GeometricShadowing(n, v, h, roughness) * GeometricShadowing(n, l, h, roughness);

	// Final formula
	// Denominator dot products partially canceled by G()!
	// See page 16: http://blog.selfshadow.com/publications/s2012-shading-course/hoffman/s2012_pbs_physics_math_notes.pdf
	return (D * F * G) / (4 * max(dot(n, v), dot(n, l)));
}

// Calculates diffuse amount based on energy conservation
//
// diffuse - Diffuse amount
// specular - Specular color (including light color)
// metalness - surface metalness amount
//
// Metals should have an albedo of (0,0,0)...mostly
// See slide 65: http://blog.selfshadow.com/publications/s2014-shading-course/hoffman/s2014_pbs_physics_math_slides.pdf
float3 DiffuseEnergyConserve(float3 diffuse, float3 specular, float metalness)
{
	return diffuse * ((1 - saturate(specular)) * (1 - metalness));
}


// === LIGHT TYPES FOR PBR LIGHTING =================================


float3 DirLightPBR(Light light, float3 normal, float3 worldPos, float3 camPos, float roughness, float metalness, float3 surfaceColor, float3 specularColor)
{
	// Get normalize direction to the light
	float3 toLight = normalize(-light.Direction);
	float3 toCam = normalize(camPos - worldPos);

	// Calculate the light amounts
	float diff = DiffusePBR(normal, toLight);
	float3 spec = MicrofacetBRDF(normal, toLight, toCam, roughness, specularColor);

	// Calculate diffuse with energy conservation
	// (Reflected light doesn't get diffused)
	float3 balancedDiff = DiffuseEnergyConserve(diff, spec, metalness);

	// Combine amount with 
	return (balancedDiff * surfaceColor + spec) * light.Intensity * light.Color;
}


float3 PointLightPBR(Light light, float3 normal, float3 worldPos, float3 camPos, float roughness, float metalness, float3 surfaceColor, float3 specularColor)
{
	// Calc light direction
	float3 toLight = normalize(light.Position - worldPos);
	float3 toCam = normalize(camPos - worldPos);

	// Calculate the light amounts
	float atten = Attenuate(light, worldPos);
	float diff = DiffusePBR(normal, toLight);
	float3 spec = MicrofacetBRDF(normal, toLight, toCam, roughness, specularColor);

	// Calculate diffuse with energy conservation
	// (Reflected light doesn't diffuse)
	float3 balancedDiff = DiffuseEnergyConserve(diff, spec, metalness);

	// Combine
	return (balancedDiff * surfaceColor + spec) * atten * light.Intensity * light.Color;
}
#endif