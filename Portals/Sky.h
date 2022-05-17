#pragma once

#include <wrl/client.h>
#include <DDSTextureLoader.h>
#include "Mesh.h"
#include "SimpleShader.h"
#include "Camera.h"
using namespace DirectX;
class Sky
{
public:
	Sky(const wchar_t* cubemapDDSFile,
		SimpleVertexShader* vs,
		SimplePixelShader* ps,
		Mesh* mesh,
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState,
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> context
	);
	void Draw(XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat);

private:
	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerOptions;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cubeMapSRV;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
	Mesh* skyMesh;
	SimplePixelShader* pixelShader;
	SimpleVertexShader* vertexShader;
};
