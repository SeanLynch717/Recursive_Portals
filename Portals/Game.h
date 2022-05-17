#pragma once

#include "DXCore.h"
#include <DirectXMath.h>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects
#include <vector>
#include "Entity.h"
#include "Camera.h"
#include "Material.h"
#include "Light.h"
#include "Sky.h"
#include "Portal.h"

class Game 
	: public DXCore
{

public:
	Game(HINSTANCE hInstance);
	~Game();

	// Overridden setup and game loop methods, which
	// will be called automatically
	void Init();
	void OnResize();
	void Update(float deltaTime, float totalTime);
	void UpdateTransforms(float deltaTime, float totalTime);
	void Draw(float deltaTime, float totalTime);
	void DrawNonPortals(XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition);
	void DrawPortals(XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition, int maxRecursion, int recursionLevel);
	void CheckPortalCollision();


private:

	// Initialization helper methods - feel free to customize, combine, etc.
	void LoadShaders(); 
	void CreateMaterials();
	void CreateBasicGeometry();

	
	// Note the usage of ComPtr below
	//  - This is a smart pointer for objects that abide by the
	//    Component Object Model, which DirectX objects do
	//  - More info here: https://github.com/Microsoft/DirectXTK/wiki/ComPtr

	// Buffers to hold actual geometry data
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> stencilWriteMask;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> innerPortalMask;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> undoStencilWriteMask;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> portalDepthWrite;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> portalBorderMask;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> gEqualRecursionStencilMask;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
	
	// Shaders and shader-related constructs
	SimplePixelShader* portalPixelShader;
	SimplePixelShader* lightingPixelShader;
	SimpleVertexShader* vertexShader;
	SimplePixelShader* skyPS;
	SimpleVertexShader* skyVS;

	DirectX::XMFLOAT3 ambientColor = DirectX::XMFLOAT3(.1, .1, .1);
	std::vector<Mesh*> meshes;
	std::vector<Entity*> entities;
	Portal* portals[6];
	std::vector<Material*> materials;
	std::vector<Light> lights;
	Camera* camera;
	Sky* skyBox;
	bool drawSkyBox;
	bool debugPortals;
	float portalCoolDown;
	bool drawWalls;
	bool portalAnimation;

	Entity* virtualCamera[2];
};

