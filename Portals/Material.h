#pragma once
#include <DirectXMath.h>
#include <Windows.h>
#include <d3d11.h>
#include <string>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects
#include <unordered_map>
#include "SimpleShader.h"
#include "Transform.h"
#include "Camera.h"
using namespace DirectX;
class Material {
public:
	Material(DirectX::XMFLOAT4 colorTint, SimplePixelShader* pixelShader, SimpleVertexShader* vertexShader, float roughness);
	~Material();

	// Getters and Setters
	DirectX::XMFLOAT4 GetColorTint();
	SimplePixelShader* GetPixelShader();
	SimpleVertexShader* GetVertexShader();
	float GetRoughnessValue();
	void SetColorTint(DirectX::XMFLOAT4 newTint);
	void SetPixelShader(SimplePixelShader* newPixelShader);
	void SetVertexShader(SimpleVertexShader* newVertexShader);
	void AddTextureSRV(std::string name, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv);
	void AddSampler(std::string name, Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler);
	void PrepareMaterial(Transform* transform, XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition);
	void UseSpecular(bool shouldUse);
	bool GetUseSpecular();

private:
	DirectX::XMFLOAT4 colorTint;
	SimplePixelShader* pixelShader;
	SimpleVertexShader* vertexShader;
	float roughness;
	bool useSpecular;
	bool useNormal;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textureSRVs; 
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11SamplerState>> samplers;
};
