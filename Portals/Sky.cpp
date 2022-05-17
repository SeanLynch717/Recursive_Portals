#include "Sky.h"

Sky::Sky(const wchar_t* cubemapDDSFile, 
	SimpleVertexShader* vs, 
	SimplePixelShader* ps, 
	Mesh* mesh, 
	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState, 
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	this->skyMesh = mesh;
	this->pixelShader = ps;
	this->vertexShader = vs;
	this->samplerOptions = samplerState;
	this->context = context;
	this->device = device;

	// Rasterizer State used to set the cull mode to see the inside of the cube.
	D3D11_RASTERIZER_DESC rastDesc = {};
	rastDesc.CullMode = D3D11_CULL_FRONT; // Draw the inside instead of the outside!
	rastDesc.FillMode = D3D11_FILL_SOLID;
	rastDesc.DepthClipEnable = true;
	device->CreateRasterizerState(&rastDesc, rasterizerState.GetAddressOf());

	// Depth stencil state to make sure we draw pixels with a depth equal to 1.
	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = true;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	device->CreateDepthStencilState(&depthDesc, depthState.GetAddressOf());

	// Load textures
	DirectX::CreateDDSTextureFromFile(device.Get(), cubemapDDSFile, 0, cubeMapSRV.GetAddressOf());
}

void Sky::Draw(XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat)
{
	// Change to the rasterizer and depth stencil state for drawing the sky.
	context->RSSetState(rasterizerState.Get());
	context->OMSetDepthStencilState(depthState.Get(), 0);
	// Set the sky shaders.
	vertexShader->SetShader();
	pixelShader->SetShader();

	// Give them proper data
	vertexShader->SetMatrix4x4("view", viewMat);
	vertexShader->SetMatrix4x4("projection", viewMat);
	vertexShader->CopyAllBufferData();

	// Send the proper resources to the pixel shader
	pixelShader->SetShaderResourceView("SkyTexture", cubeMapSRV);
	pixelShader->SetSamplerState("BasicSampler", samplerOptions);

	// Draw the sky box.
	skyMesh->Draw();

	// Reset my rasterizer state to the default
	context->RSSetState(0); // Null (or 0) puts back the defaults
	context->OMSetDepthStencilState(0, 0);
}
