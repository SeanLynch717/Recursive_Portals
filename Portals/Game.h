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

using namespace std;

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
	void TryPlacePortal(int id);
	bool RayTriangleIntersect(
		DirectX::XMVECTOR rayOrigin,    // Point 1 of line
		DirectX::XMVECTOR rayDir,       // Direction (Point 2 - Point 1)
		DirectX::XMVECTOR V0,           // Triangle Vertex 0
		DirectX::XMVECTOR V1,           // Triangle Vertex 1
		DirectX::XMVECTOR V2,           // Triangle Vertex 2
		DirectX::XMFLOAT3& outPoint		// The intersection point in 3D space
	);


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
	vector<Mesh*> meshes;
	unordered_map<string, Entity*> entities;
	unordered_map<string, Portal*> portals;
	unordered_map<string, Material*> materials;
	vector<Light> lights;
	Camera* camera;
	Sky* skyBox;
	bool drawSkyBox;
	bool debugPortals;
	float portalCoolDown;
	float portalPlacementCoolDown;
	bool drawWalls = true;
	bool portalAnimation;
	float portalOffset = 0.5f;
	float maxPortalPlacementDistance = 50.0f;

	// Portal tweens
	float leftPortalTween = 1.0f;
	float rightPortalTween = 1.0f;
	float portalTweenSpeed = 5.0f;

	Entity* virtualCamera[2];
};

