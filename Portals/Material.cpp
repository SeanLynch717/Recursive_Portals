#include "Material.h"
#include <iostream>
using namespace std;

Material::Material(DirectX::XMFLOAT4 colorTint, SimplePixelShader* pixelShader, SimpleVertexShader* vertexShader, float roughness)
{
	this->colorTint = colorTint;
	this->pixelShader = pixelShader;
	this->vertexShader = vertexShader;
	if (roughness > 1.0f)
		this->roughness = 1.0f;
	else if (roughness < 0.0f)
		this->roughness = 0.0f;
	else
		this->roughness = roughness;
}

Material::~Material()
{
}

DirectX::XMFLOAT4 Material::GetColorTint()
{
	return colorTint;
}

void Material::SetColorTint(DirectX::XMFLOAT4 newTint)
{
	this->colorTint = newTint;
}

SimplePixelShader* Material::GetPixelShader()
{
	return pixelShader;
}

void Material::SetPixelShader(SimplePixelShader* newPixelShader)
{
	this->pixelShader = newPixelShader;
}

SimpleVertexShader* Material::GetVertexShader()
{
	return vertexShader;
}

float Material::GetRoughnessValue()
{
	return roughness;
}

void Material::SetVertexShader(SimpleVertexShader* newVertexShader)
{
	this->vertexShader = newVertexShader;
}

void Material::AddTextureSRV(std::string name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv)
{
	if (name.find("Roughness") != std::string::npos) useSpecular = true;
	else if (name.find("Normal") != std::string::npos) useNormal = true;
	textureSRVs.insert({ name, srv });
}

void Material::AddSampler(std::string name, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler)
{
	samplers.insert({ name, sampler });
}

void Material::PrepareMaterial(Transform* transform, XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition)
{
	// Set the vertex and pixel shaders corresponding to the individual material of the mesh.
	vertexShader->SetShader();

	// Data being sent to GPU
	vertexShader->SetMatrix4x4("world", transform->GetWorldMatrix());
	vertexShader->SetMatrix4x4("view", viewMat);
	vertexShader->SetMatrix4x4("projection", projMat);
	vertexShader->SetMatrix4x4("worldInverseTranspose", transform->GetWorldInverseTranspose());
	vertexShader->CopyAllBufferData();

	if (pixelShader == NULL) return;
	pixelShader->SetShader();
	pixelShader->SetFloat4("colorTint", GetColorTint());
	pixelShader->SetFloat("roughness", GetRoughnessValue());
	pixelShader->SetFloat3("cameraPosition", cameraPosition);
	pixelShader->SetInt("hasSpecular", useSpecular ? 1 : 0);
	pixelShader->SetInt("hasNormal", useNormal ? 1 : 0);
	pixelShader->CopyAllBufferData();
	for (auto& t : textureSRVs) { pixelShader -> SetShaderResourceView(t.first.c_str(), t.second); }
	for (auto& s : samplers) { pixelShader->SetSamplerState(s.first.c_str(), s.second); }
}

void Material::UseSpecular(bool shouldUse)
{
	useSpecular = shouldUse;
}

bool Material::GetUseSpecular()
{
	return this->useSpecular;
}
