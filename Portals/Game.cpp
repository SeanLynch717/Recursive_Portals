#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "WICTextureLoader.h"
// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>
#include <math.h>
#include <iostream>
#include "MeshFactory.h"

// For the DirectX Math library
using namespace DirectX;

#define PI 3.14159265
// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance) :
	DXCore(
		hInstance,		   // The application's handle
		"DirectX Game",	   // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true)			   // Show extra stats (fps) in title bar?
{
	camera = 0;

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.\n");
#endif

}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Note: Since we're using smart pointers (ComPtr),
	// we don't need to explicitly clean up those DirectX objects
	// - If we weren't using smart pointers, we'd need
	//   to call Release() on each DirectX object created in Game
	for (Mesh* mesh : meshes) {
		delete mesh;
	}
	for (const auto& pair: entities) {
		delete pair.second;
	}
	for (const auto& pair : materials) {
		delete pair.second;
	}
	for (const auto& pair : portals) {
		delete pair.second;
	}
	delete camera;
	delete vertexShader;
	delete portalPixelShader;
	delete lightingPixelShader;
	delete skyVS;
	delete skyPS;
	delete skyBox;
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{
	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	//  - You'll be expanding and/or replacing these later
	LoadShaders();
	CreateMaterials();
	CreateBasicGeometry();
	ShowCursor(FALSE);
	// Create the skybox
	//skyBox = new Sky(GetFullPathTo_Wide(L"../../Assets/Textures/Skies/SunnyCubeMap.dds").c_str(), skyVS, skyPS, meshes[0], sampler, device, context);

	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the camera once we have the aspect ratio
	camera = new Camera(0, 2, 0, 5.0f, .15f, XM_PIDIV4, (float)width / height, hWnd);
	camera->GetTransform()->SetPitchYawRoll(0, PI / 2, 0);
	drawSkyBox = true;
	debugPortals = true;

	// Create various depth stencil descriptions for portal rendering
	{
		CD3D11_DEPTH_STENCIL_DESC depthStencilDesc = CD3D11_DEPTH_STENCIL_DESC{ CD3D11_DEFAULT{} };
		depthStencilDesc.DepthEnable = FALSE;									// Disable color, depth drawing, and depth test
		depthStencilDesc.StencilEnable = TRUE;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL; 	// Fail stencil test where we should be drawing inner portal
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_INCR;		// Increment stencil value on fail (on area of inner portal)
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.StencilReadMask = 0xFF;								// Enable reading everywhere (maybe dont need this line)
		depthStencilDesc.StencilWriteMask = 0xFF;								// Enable writing everywhere
		device->CreateDepthStencilState(&depthStencilDesc, stencilWriteMask.GetAddressOf());

		depthStencilDesc.DepthEnable = TRUE;									// Enable the depth test
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;						// Usual depth comparison
		depthStencilDesc.StencilEnable = TRUE;									// Enable the stencil test
		depthStencilDesc.StencilWriteMask = 0x00;								// Disable stencil writing
		depthStencilDesc.StencilReadMask = 0xFF;								// Enable stencil reading
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		device->CreateDepthStencilState(&depthStencilDesc, innerPortalMask.GetAddressOf());

		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;			// Disable depth writing
		depthStencilDesc.StencilEnable = TRUE;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_DECR;		// Decrement stencil value on fail (on area of inner portal)
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		device->CreateDepthStencilState(&depthStencilDesc, undoStencilWriteMask.GetAddressOf());

		depthStencilDesc.StencilEnable = FALSE;
		depthStencilDesc.StencilReadMask = 0x00;
		depthStencilDesc.StencilWriteMask = 0x00;
		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		device->CreateDepthStencilState(&depthStencilDesc, portalDepthWrite.GetAddressOf());

		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		depthStencilDesc.StencilEnable = TRUE;
		depthStencilDesc.StencilWriteMask = 0x00;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
		device->CreateDepthStencilState(&depthStencilDesc, portalBorderMask.GetAddressOf());

		depthStencilDesc.DepthEnable = TRUE;
		depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
		depthStencilDesc.StencilEnable = TRUE;
		depthStencilDesc.StencilWriteMask = 0x00;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_LESS_EQUAL;
		device->CreateDepthStencilState(&depthStencilDesc, gEqualRecursionStencilMask.GetAddressOf());
	}
	// Create alpha transparency state for rendering edges of the portals.
	{
		D3D11_BLEND_DESC blendStateDesc{};
		blendStateDesc.AlphaToCoverageEnable = FALSE;
		blendStateDesc.IndependentBlendEnable = FALSE;
		blendStateDesc.RenderTarget[0].BlendEnable = TRUE;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0x0f;
		device->CreateBlendState(&blendStateDesc, blendState.GetAddressOf());
		float bf[] = { 0.f,0.f,0.f,0.f };
		context->OMSetBlendState(blendState.Get(), nullptr, 0xFFFFFFFF);
	}
	portalCoolDown = 10;
}

// --------------------------------------------------------
// Loads shaders from compiled shader object (.cso) files
// and also created the Input Layout that describes our 
// vertex data to the rendering pipeline. 
// - Input Layout creation is done here because it must 
//    be verified against vertex shader byte code
// - We'll have that byte code already loaded below
// --------------------------------------------------------
void Game::LoadShaders()
{
	vertexShader = new SimpleVertexShader(device.Get(), context.Get(), GetFullPathTo_Wide(L"VertexShader.cso").c_str());
	portalPixelShader = new SimplePixelShader(device.Get(), context.Get(), GetFullPathTo_Wide(L"PortalPS.cso").c_str());
	lightingPixelShader = new SimplePixelShader(device.Get(), context.Get(), GetFullPathTo_Wide(L"LightingPS.cso").c_str());
	skyVS = new SimpleVertexShader(device.Get(), context.Get(), GetFullPathTo_Wide(L"SkyVS.cso").c_str());
	skyPS = new SimplePixelShader(device.Get(), context.Get(), GetFullPathTo_Wide(L"SkyPS.cso").c_str());
}

// Create the basic materials for assignment 5
void Game::CreateMaterials()
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobblestoneAlbedoRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobblestoneSpecularRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobblestoneNormalRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> cobblestoneMetalnessRSV;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorAlbedoRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorSpecularRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorNormalRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> floorMetalnessRSV;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metalAlbedoRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metalSpecularRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metalNormalRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> metalMetalnessRSV;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodAlbedoRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodSpecularRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodNormalRSV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> woodMetalnessRSV;

	// Load textures.
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/cobblestone_albedo.png").c_str(), 0, cobblestoneAlbedoRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/cobblestone_roughness.png").c_str(), 0, cobblestoneSpecularRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/cobblestone_normals.png").c_str(), 0, cobblestoneNormalRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/cobblestone_metal.png").c_str(), 0, cobblestoneMetalnessRSV.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/paint_albedo.png").c_str(), 0, floorAlbedoRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/paint_roughness.png").c_str(), 0, floorSpecularRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/paint_normals.png").c_str(), 0, floorNormalRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/paint_metal.png").c_str(), 0, floorMetalnessRSV.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/floor_albedo.png").c_str(), 0, metalAlbedoRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/floor_roughness.png").c_str(), 0, metalSpecularRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/floor_normals.png").c_str(), 0, metalNormalRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/floor_metal.png").c_str(), 0, metalMetalnessRSV.GetAddressOf());

	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/wood_albedo.png").c_str(), 0, woodAlbedoRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/wood_roughness.png").c_str(), 0, woodSpecularRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/wood_normals.png").c_str(), 0, woodNormalRSV.GetAddressOf());
	CreateWICTextureFromFile(device.Get(), context.Get(), GetFullPathTo_Wide(L"../../Assets/Textures/wood_metal.png").c_str(), 0, woodMetalnessRSV.GetAddressOf());

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; // What happens outside the 0-1 uv range?
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;		// How do we handle sampling "between" pixels?
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());

	// Cobblestone Material
	materials.insert({ "cobblestone", new Material(XMFLOAT4(1, 1, 1, 0), lightingPixelShader, vertexShader, 0.0f) });
	materials["cobblestone"]->AddSampler("BasicSampler", sampler);
	materials["cobblestone"]->AddTextureSRV("Albedo", cobblestoneAlbedoRSV);
	materials["cobblestone"]->AddTextureSRV("RoughnessMap", cobblestoneSpecularRSV);
	materials["cobblestone"]->AddTextureSRV("NormalMap", cobblestoneNormalRSV);
	materials["cobblestone"]->AddTextureSRV("MetalnessMap", cobblestoneMetalnessRSV);

	// Floor Material
	materials.insert({"floor", new Material(XMFLOAT4(1, 1, 1, 0), lightingPixelShader, vertexShader, 0.0f)});
	materials["floor"]->AddSampler("BasicSampler", sampler);
	materials["floor"]->AddTextureSRV("Albedo", floorAlbedoRSV);
	materials["floor"]->AddTextureSRV("RoughnessMap", floorSpecularRSV);
	materials["floor"]->AddTextureSRV("NormalMap", floorNormalRSV);
	materials["floor"]->AddTextureSRV("MetalnessMap", floorMetalnessRSV);

	// Metal Material
	materials.insert({ "metal", new Material(XMFLOAT4(1, 1, 1, 0), lightingPixelShader, vertexShader, 0.0f) });
	materials["metal"]->AddSampler("BasicSampler", sampler);
	materials["metal"]->AddTextureSRV("Albedo", metalAlbedoRSV);
	materials["metal"]->AddTextureSRV("RoughnessMap", metalSpecularRSV);
	materials["metal"]->AddTextureSRV("NormalMap", metalNormalRSV);
	materials["metal"]->AddTextureSRV("MetalnessMap", metalMetalnessRSV);

	materials.insert({ "wood", new Material(XMFLOAT4(1, 1, 1, 0), lightingPixelShader, vertexShader, 0.0f) });
	materials["wood"]->AddSampler("BasicSampler", sampler);
	materials["wood"]->AddTextureSRV("Albedo", woodAlbedoRSV);
	materials["wood"]->AddTextureSRV("RoughnessMap", woodSpecularRSV);
	materials["wood"]->AddTextureSRV("NormalMap", woodNormalRSV);
	materials["wood"]->AddTextureSRV("MetalnessMap", woodMetalnessRSV);

	// Portal Material
	materials.insert({"portal", new Material(XMFLOAT4(1, 1, 1, 0), portalPixelShader, vertexShader, 0.0f)});
	materials["portal"]->AddSampler("BasicSampler", sampler);
	materials["portal"]->GetPixelShader()->SetFloat("borderThickness", portalBorderThickness / 2);
}

// --------------------------------------------------------
// Creates the geometry we're going to draw
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	Mesh* mesh1 = new Mesh(GetFullPathTo("../../Assets/Models/cube.obj").c_str(), device, context);
	meshes.push_back(mesh1);
	Mesh* mesh2 = new Mesh(GetFullPathTo("../../Assets/Models/sphere.obj").c_str(), device, context);
	meshes.push_back(mesh2);
	Mesh* portalMesh = new Mesh(GetFullPathTo("../../Assets/Models/quad.obj").c_str(), device, context);
	meshes.push_back(portalMesh);
	Mesh* newPortalMesh = MeshFactory::CreateCircleMesh(400, device, context);
	meshes.push_back(newPortalMesh);

	// Create scene mesh.
	entities.insert({ "floor", new Entity(meshes[0], materials["wood"])});
	entities["floor"]->GetTransform()->SetScale(20, .01f, 20);
	entities["floor"]->GetTransform()->MoveAbsolute(0, 0, 0);
	entities.insert({ "pos_z_wall", new Entity(meshes[0], materials["cobblestone"])});
	entities["pos_z_wall"]->GetTransform()->SetScale(20, 20, 0.001f);
	entities["pos_z_wall"]->GetTransform()->SetPitchYawRoll(0, 0, PI / 2);
	entities["pos_z_wall"]->GetTransform()->SetPosition(0, 10, 10);
	entities.insert({ "neg_z_wall", new Entity(meshes[0], materials["cobblestone"])});
	entities["neg_z_wall"]->GetTransform()->SetScale(20, 20, 0.001f);
	entities["neg_z_wall"]->GetTransform()->SetPitchYawRoll(0, 0, PI / 2);
	entities["neg_z_wall"]->GetTransform()->SetPosition(0, 10, -10);
	entities.insert({"pos_x_wall", new Entity(meshes[0], materials["cobblestone"])});
	entities["pos_x_wall"]->GetTransform()->SetScale(0.001f, 20, 20);
	entities["pos_x_wall"]->GetTransform()->SetPosition(10, 10, 0);
	entities.insert({ "neg_x_wall", new Entity(meshes[0], materials["cobblestone"]) });
	entities["neg_x_wall"]->GetTransform()->SetScale(0.001f, 20, 20);
	entities["neg_x_wall"]->GetTransform()->SetPosition(-10, 10, 0);
	entities.insert({ "sphere", new Entity(meshes[1], materials["metal"])});
	entities["sphere"]->GetTransform()->SetScale(1, 1, 1);
	entities["sphere"]->GetTransform()->MoveAbsolute(0, 2, 5);
	
	// First set of portals
	/*portals.insert({ "portal_set_1_a", new Portal(meshes[3], materials["portal"], 0, XMFLOAT3(1, 0.6f, 0)) });
	portals["portal_set_1_a"]->GetTransform()->MoveAbsolute(3.5f, 3, 10 - portalOffset);
	portals["portal_set_1_a"]->GetTransform()->SetPitchYawRoll(0, PI, 0);
	portals["portal_set_1_a"]->GetTransform()->SetScale(1, 2, 1);
	portals.insert({ "portal_set_1_b", new Portal(meshes[3], materials["portal"], 1, XMFLOAT3(1, 0.6f, 0)) });
	portals["portal_set_1_b"]->GetTransform()->MoveAbsolute(3.5f, 3, -10 + portalOffset);
	portals["portal_set_1_b"]->GetTransform()->SetPitchYawRoll(0, 0, 0);
	portals["portal_set_1_b"]->GetTransform()->SetScale(1, 2, 1);
	portals["portal_set_1_a"]->SetDestination(portals["portal_set_1_b"]);
	portals["portal_set_1_b"]->SetDestination(portals["portal_set_1_a"]);*/

	// Second set of portals
	//portals.insert({ "portal_set_2_a", new Portal(meshes[3], materials["portal"], 0, XMFLOAT3(0, 0, 1)) });
	//portals["portal_set_2_a"]->GetTransform()->MoveAbsolute(10 - portalOffset, 3, 0);
	//portals["portal_set_2_a"]->GetTransform()->SetPitchYawRoll(0, - PI / 2, 0);
	//portals["portal_set_2_a"]->GetTransform()->SetScale(1, 2, 1);
	//portals.insert({ "portal_set_2_b", new Portal(meshes[3], materials["portal"], 1, XMFLOAT3(0, 0, 1)) });
	//portals["portal_set_2_b"]->GetTransform()->MoveAbsolute(-10 + portalOffset, 3, 0);
	//portals["portal_set_2_b"]->GetTransform()->SetScale(1, 2, 1);
	//portals["portal_set_2_b"]->GetTransform()->SetPitchYawRoll(0, PI / 2, 0);
	//portals["portal_set_2_a"]->SetDestination(portals["portal_set_2_b"]);
	//portals["portal_set_2_b"]->SetDestination(portals["portal_set_2_a"]);

	// Third set of portals
	//portals.insert({ "portal_set_3_a", new Portal(meshes[3], materials["portal"], 0, XMFLOAT3(247.0f / 255, 208.0f / 255, 2.0f / 255)) });
	//portals["portal_set_3_a"]->GetTransform()->MoveAbsolute(-3.5f, 3, 10 - portalOffset);
	//portals["portal_set_3_a"]->GetTransform()->SetPitchYawRoll(0, PI, 0);
	//portals["portal_set_3_a"]->GetTransform()->SetScale(1, 2, 1);
	//portals.insert({ "portal_set_3_b", new Portal(meshes[3], materials["portal"], 1, XMFLOAT3(247.0f / 255, 208.0f / 255, 2.0f / 255)) });
	//portals["portal_set_3_b"]->GetTransform()->MoveAbsolute(-3.5f, 3, -10 + portalOffset);
	//portals["portal_set_3_b"]->GetTransform()->SetPitchYawRoll(0, 0, 0);
	//portals["portal_set_3_b"]->GetTransform()->SetScale(1, 2, 1);
	//portals["portal_set_3_a"]->SetDestination(portals["portal_set_3_b"]);
	//portals["portal_set_3_b"]->SetDestination(portals["portal_set_3_a"]);
	//portals[1] = new Portal(meshes[2], materials[3], 1, XMFLOAT3(0, 0, 1));
	//portals[1]->GetTransform()->MoveAbsolute(10 - portalOffset, 2, 8);
	//portals[1]->GetTransform()->SetPitchYawRoll(0, -PI/2, 0);
	//portals[1]->GetTransform()->SetScale(1, 2, 1);
	//portals[0]->SetDestination(portals[1]);
	//portals[1]->SetDestination(portals[0]);

	//// Second set of portals
	//portals[2] = new Portal(meshes[2], materials[3], 0, XMFLOAT3(1, 0.6f, 0));
	//portals[2]->GetTransform()->MoveAbsolute(-4, 2, 10 - portalOffset);
	//portals[2]->GetTransform()->SetPitchYawRoll(0, PI, 0);
	//portals[2]->GetTransform()->SetScale(1, 2, 1);
	//portals[3] = new Portal(meshes[2], materials[3], 1, XMFLOAT3(1, 0.6f, 0));
	//portals[3]->GetTransform()->MoveAbsolute(-4, 2, 0 + portalOffset);
	//portals[3]->GetTransform()->SetPitchYawRoll(0, 0, 0);
	//portals[3]->GetTransform()->SetScale(1, 2, 1);
	//portals[2]->SetDestination(portals[3]);
	//portals[3]->SetDestination(portals[2]);

	//// Third set of portals
	//portals[4] = new Portal(meshes[2], materials[3], 0, XMFLOAT3(0, 1, 0));
	//portals[4]->GetTransform()->MoveAbsolute(10 - portalOffset, 2, 3);
	//portals[4]->GetTransform()->SetPitchYawRoll(0, -PI/2, 0);
	//portals[4]->GetTransform()->SetScale(1, 2, 1);
	//portals[5] = new Portal(meshes[2], materials[3], 1, XMFLOAT3(0, 1, 0));
	//portals[5]->GetTransform()->MoveAbsolute(-10 + portalOffset, 2, 3);
	//portals[5]->GetTransform()->SetPitchYawRoll(0, PI/2, 0);
	//portals[5]->GetTransform()->SetScale(1, 2, 1);
	//portals[4]->SetDestination(portals[5]);
	//portals[5]->SetDestination(portals[4]);

	// For debugging the virtual cameras position.
	//virtualCamera[0] = new Entity(meshes[0], materials[0]);
	//virtualCamera[0]->GetTransform()->MoveAbsolute(0, 0, 0);
	//virtualCamera[1] = new Entity(meshes[0], materials[0]);
	//virtualCamera[1]->GetTransform()->MoveAbsolute(0, 0, 0);

	// Create basic lighting
	Light dirLight1 = {};
	dirLight1.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight1.Direction = XMFLOAT3(1, 0, 0);
	dirLight1.Intensity = 1.0f;
	dirLight1.Color = XMFLOAT3(1, 1, 1);

	Light dirLight2 = {};
	dirLight2.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight2.Direction = XMFLOAT3(-1/sqrt(3), -1/sqrt(3), 1/sqrt(3));
	dirLight2.Intensity = 1.0f;
	dirLight2.Color = XMFLOAT3(1, 1, 1);

	Light dirLight3 = {};
	dirLight3.Type = LIGHT_TYPE_DIRECTIONAL;
	dirLight3.Direction = XMFLOAT3(-1/sqrt(2), 1/sqrt(2), 0);
	dirLight3.Intensity = 1.0f;
	dirLight3.Color = XMFLOAT3(1, 1, 1);

	Light pointLight1 = {};
	pointLight1.Type = LIGHT_TYPE_POINT;
	pointLight1.Range = 100.0f;
	pointLight1.Intensity = 1.0f;
	pointLight1.Position = XMFLOAT3(-10, 13, 0);
	pointLight1.Color = XMFLOAT3(1, 1, 1);

	Light pointLight2 = {};
	pointLight2.Type = LIGHT_TYPE_POINT;
	pointLight2.Range = 100.0f;
	pointLight2.Intensity = 1.0f;
	pointLight2.Position = XMFLOAT3(-6, -15, 0);
	pointLight2.Color = XMFLOAT3(1, 1, 1);

	lights.push_back(dirLight1);
	lights.push_back(dirLight2);
	lights.push_back(dirLight3);
	lights.push_back(pointLight1);
	lights.push_back(pointLight2);
}

// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Ensure we update the camera whenever the window resizes
	// Note: This could trigger before Init(), so ensure
	// our pointer is valid first.
	if (camera)
	{
		float aspectRatio = (float)width / height;
		camera->UpdateProjectionMatrix(aspectRatio);
	}
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Example input checking: Quit if the escape key is pressed
	if (Input::GetInstance().KeyDown(VK_ESCAPE))
		Quit();

	UpdateTransforms(deltaTime, totalTime);

	float fov = camera->GetFoV();
	if (Input::GetInstance().KeyDown('P')) {
		fov += 1 * deltaTime;
		camera->SetFoV(fov);
	}
	if (Input::GetInstance().KeyDown('O')) {
		fov -= 1 * deltaTime;
		camera->SetFoV(fov);
	}
	if (Input::GetInstance().KeyPress('L')) {
		drawWalls = !drawWalls;
	}
	if (Input::GetInstance().KeyPress('M')) {
		camera->ToggleMouse();
	}
	if (portalPlacementCoolDown > 0.5f) {
		if (Input::GetInstance().MouseLeftDown()) {
			TryPlacePortal(0);
		}
		if (Input::GetInstance().MouseRightDown()) {
			TryPlacePortal(1);
		}
	}
	//if (Input::GetInstance().KeyPress('B')) {
	//	drawSkyBox = !drawSkyBox;
	//}
	
	// Portal tween
	if (leftPortalTween != 1.0f) {
		leftPortalTween += portalTweenSpeed * deltaTime;
		if (leftPortalTween > 1.0f) {
			leftPortalTween = 1.0f;
		}
	}
	if (rightPortalTween != 1.0f) {
		rightPortalTween += portalTweenSpeed * deltaTime;
		if (rightPortalTween > 1.0f) {
			rightPortalTween = 1.0f;
		}
	}


	CheckPortalCollision();
	portalCoolDown += deltaTime;
	portalPlacementCoolDown += deltaTime;
	camera->Update(deltaTime);
}

void Game::UpdateTransforms(float deltaTime, float totalTime)
{
	XMFLOAT3 pos = entities["sphere"]->GetTransform()->GetPosition();
	entities["sphere"]->GetTransform()->SetPosition(2 * sin(totalTime), pos.y, pos.z);
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Background color (Cornflower Blue in this case) for clearing
	const float color[4] = { .39f, .58f, .92f, 1 };

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	context->ClearRenderTargetView(backBufferRTV.Get(), color);
	context->ClearDepthStencilView(
		depthStencilView.Get(),
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);

	// Draw Portals
	DrawPortals(camera->GetView(), camera->GetProjection(), camera->GetTransform()->GetPosition(), 3, 0);
	
	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(0, 0);

	// Due to the usage of a more sophisticated swap chain,
	// the render target must be re-bound after every call to Present()
	context->OMSetRenderTargets(1, backBufferRTV.GetAddressOf(), depthStencilView.Get());
}

// Draw anything that is a non-portal.
void Game::DrawNonPortals(XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition)
{
	for (auto& pair : entities) {
		// Skip drawing walls
		if (!drawWalls && pair.first.find("wall") != string::npos) {
			continue;
		}
		Entity* entity = pair.second;
		SimplePixelShader* ps = entity->GetMaterial()->GetPixelShader();
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetFloat3("ambientColor", ambientColor);
		entity->Draw(context, viewMat, projMat, cameraPosition);
	}
}

// Draw the portals by calculating the virtual cameras view and clipped projection matrix, and using the stencil buffer.
void Game::DrawPortals(XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition, int maxRecursion, int recursionLevel)
{
	for (const auto& pair : portals) {
		Portal* portal = pair.second;

		// No destination portal set, exit early!
		if (portal->GetDestination() == nullptr) {
			continue;
		}

		// Set portal scale based on tween value for drawing to the stencil buffer
		float scale = pair.first == "portal_0" ? leftPortalTween : rightPortalTween;
		XMFLOAT3 originalScale = portal->GetTransform()->GetScale();
		portal->GetTransform()->SetScale(1.0f * (sin(scale * PI / 2) - portalBorderThickness), 2.0f * (sin(scale * PI / 2) - portalBorderThickness), 1.0f);

		// Set Depth Stencil State and Draw portal to stencil buffer. 
		// This isn't actually drawing the portal, but it is incrementing the stencil buffer values in the area of the screen where the portal is.
		context->OMSetDepthStencilState(stencilWriteMask.Get(), recursionLevel);
		portal->UnbindPSAndDraw(context, viewMat, projMat, cameraPosition);
		// Revert portal scale
		portal->GetTransform()->SetScale(originalScale.x, originalScale.y, originalScale.z);

		// Calculate the view from the perspective of the out portal
		XMMATRIX rotationMatrix = XMMatrixRotationAxis(XMVectorSet(0, 1, 0, 0), PI);
		XMFLOAT4X4 destinationWorldMatrix = portal->GetDestination()->GetTransform()->GetWorldMatrix();
		XMFLOAT4X4 currentWorldMatrix = portal->GetTransform()->GetWorldMatrix();
		XMMATRIX V = XMMatrixInverse(0, XMLoadFloat4x4(&destinationWorldMatrix))
			* rotationMatrix
			* XMLoadFloat4x4(&currentWorldMatrix)
			* XMLoadFloat4x4(&viewMat);
		XMFLOAT4X4 viewDest;
		XMStoreFloat4x4(&viewDest, V);

		// Calculate Oblique Clipped Projection Matrix.
		XMFLOAT4X4 newProj = portal->GetDestination()->ClippedProjectionMatrix(viewDest, projMat);

		// Calculate new camera position
		XMFLOAT3 relPos = portal->GetTransform()->InverseTransformPoint(camera->GetTransform()->GetPosition());
		XMStoreFloat3(
			&relPos,
			XMVector3Transform(XMLoadFloat3(&relPos), rotationMatrix)
		);
		relPos = portal->GetDestination()->GetTransform()->TransformPoint(relPos);

		// Base case: 
		if (recursionLevel == maxRecursion) {
			// Set depth stencil state,
			context->OMSetDepthStencilState(innerPortalMask.Get(), recursionLevel + 1);
			// Clear the depth buffer
			context->ClearDepthStencilView(
				depthStencilView.Get(),
				D3D11_CLEAR_DEPTH,
				1.0f,
				0);
			// Draw world constrained to the inner portal
			DrawNonPortals(viewDest, newProj, relPos);
			
			// Draw inner most portal outline
			portal->GetMaterial()->GetPixelShader()->SetInt("drawRecursive", 0);
			portal->GetMaterial()->GetPixelShader()->SetFloat("scale", scale);
			portal->GetMaterial()->GetPixelShader()->SetFloat3("borderColor", portal->GetBorderColor());
			portal->Draw(context, viewDest, newProj, relPos);
		}
		// Recursive case:
		else {
			// Draw portals recursively
			DrawPortals(viewDest, newProj, relPos, maxRecursion, recursionLevel + 1);
		}

		// Set depth stencil state
		context->OMSetDepthStencilState(undoStencilWriteMask.Get(), recursionLevel + 1);
		// Clear the depth buffer
		context->ClearDepthStencilView(
			depthStencilView.Get(),
			D3D11_CLEAR_DEPTH,
			1.0f,
			0);
		// Draw portal into stencil buffer. The undoStencilWriteMask decrements the stencil values where the portal is
		// eventually returning to a buffer full of zeroes.
		portal->GetTransform()->SetScale(1.0f * (sin(scale * PI / 2) - portalBorderThickness), 2.0f * (sin(scale * PI / 2) - portalBorderThickness), 1.0f);
		portal->UnbindPSAndDraw(context, viewMat, projMat, cameraPosition);
		portal->GetTransform()->SetScale(originalScale.x, originalScale.y, originalScale.z);
	}
	
	// Clear the depth buffer
	context->ClearDepthStencilView(
		depthStencilView.Get(),
		D3D11_CLEAR_DEPTH,
		1.0f,
		0);

	// Set depth stencil state
	context->OMSetDepthStencilState(portalDepthWrite.Get(), 0);

	// Draw each portal (a plane) into the depth buffer
	for (auto& pair : portals) {
		// Tween scale for depth drawing
		float scale = pair.first == "portal_0" ? leftPortalTween : rightPortalTween;
		XMFLOAT3 originalScale = pair.second->GetTransform()->GetScale();
		pair.second->GetTransform()->SetScale(1.0f * (sin(scale * PI / 2) - portalBorderThickness), 2.0f * (sin(scale * PI / 2) - portalBorderThickness), 1.0f);
		pair.second->UnbindPSAndDraw(context, viewMat, projMat, cameraPosition);
		// Revert portal scale
		pair.second->GetTransform()->SetScale(originalScale.x, originalScale.y, originalScale.z);
	}
	
	// Set depth stencil state
	context->OMSetDepthStencilState(portalBorderMask.Get(), recursionLevel);

	// Draw portal outline
	for (auto& pair : portals) {
		Portal* portal = pair.second;
		float scale = pair.first == "portal_0" ? leftPortalTween : rightPortalTween;
		portal->GetMaterial()->GetPixelShader()->SetInt("drawRecursive", 1);
		portal->GetMaterial()->GetPixelShader()->SetFloat("scale", scale);
		portal->GetMaterial()->GetPixelShader()->SetFloat3("borderColor", portal->GetBorderColor());
		portal->Draw(context, viewMat, projMat, cameraPosition);
	}

	context->OMSetDepthStencilState(gEqualRecursionStencilMask.Get(), recursionLevel);
	// Only draw where stencil value >= recursion level
	// This prevents drawing outside of outside of this level
	DrawNonPortals(viewMat, projMat, cameraPosition);
}

// This method checks if the camera is colliding with a portal, and teleports the camera to the destination portal.
void Game::CheckPortalCollision()
{
	if (portalCoolDown < 1.0f) return;
	XMFLOAT3 cameraPos = camera->GetTransform()->GetPosition();

	for (auto& pair : portals) {
		Portal* portal = pair.second;
		// No destination portal set, exit early!
		if (portal->GetDestination() == nullptr) {
			continue;
		}
		// Calculate dot product of the cameras position relative to the portal, and the portal's forward vector.
		XMFLOAT3 portalPos = portal->GetTransform()->GetPosition();
		XMFLOAT3 diff = XMFLOAT3(cameraPos.x - portalPos.x, cameraPos.y - portalPos.y, cameraPos.z - portalPos.z);
		XMFLOAT3 forward = portal->GetTransform()->GetForward();
		float dot = XMVector3Dot(XMLoadFloat3(&diff), XMLoadFloat3(&forward)).m128_f32[0];

		// Project the cameras position onto the portal plane's axes, and calculate the magnitude of that projection.
		XMFLOAT3 right = portal->GetTransform()->GetRight();
		XMFLOAT3 up = portal->GetTransform()->GetUp();
		float rightDot = (right.x * diff.x) + (right.y * diff.y) + (right.z * diff.z);
		float upDot = (up.x * diff.x) + (up.y * diff.y) + (up.z * diff.z);
		XMFLOAT3 planeProj = XMFLOAT3((right.x * rightDot) + (up.x * upDot), (right.y * rightDot) + (up.y * upDot), (right.z * rightDot) + (up.z * upDot));
		float dist = sqrt(pow(planeProj.x, 2) + pow(planeProj.y, 2) + pow(planeProj.z, 2));

		// Check if the camera is on the negative side of the plane, AND the magnitude of the cameras projection 
		// onto the portal plane is within bounds of the portals frame.
		if (dot > -0.1f && dot <= 0.5f && dist <= 1) {
			XMFLOAT3 destPos = portal->GetDestination()->GetTransform()->GetPosition();
			XMFLOAT3 portalRot = portal->GetTransform()->GetPitchYawRoll();
			XMFLOAT3 destRot = portal->GetDestination()->GetTransform()->GetPitchYawRoll();
			XMFLOAT3 cameraRot = camera->GetTransform()->GetPitchYawRoll();
			float rotDiff = cameraRot.y + PI - portalRot.y;
			XMVECTOR xmDiff = XMLoadFloat3(&diff);

			// Calculate local offsets relative to the ENTRANCE portal's axes
			float xOffset = XMVector3Dot(xmDiff, XMLoadFloat3(&right)).m128_f32[0];
			float yOffset = XMVector3Dot(xmDiff, XMLoadFloat3(&up)).m128_f32[0];
			float zOffset = XMVector3Dot(xmDiff, XMLoadFloat3(&forward)).m128_f32[0];

			// Reconstruct position relative to the DESTINATION portal's axes
			// We flip the X and Z offsets because entering the 'front' means exiting the 'front'
			XMFLOAT3 destRight = portal->GetDestination()->GetTransform()->GetRight();
			XMFLOAT3 destUp = portal->GetDestination()->GetTransform()->GetUp();
			XMFLOAT3 destForward = portal->GetDestination()->GetTransform()->GetForward();
			XMVECTOR newPos = XMLoadFloat3(&destPos) - (XMLoadFloat3(&destRight) * xOffset) + (XMLoadFloat3(&destUp) * yOffset) - (XMLoadFloat3(&destForward) * zOffset);

			// Apply the position and the same rotation logic you already have
			XMStoreFloat3(&cameraPos, newPos);
			camera->GetTransform()->SetPosition(cameraPos.x, cameraPos.y, cameraPos.z);
			camera->GetTransform()->SetPitchYawRoll(cameraRot.x, destRot.y + rotDiff, cameraRot.z);
			portalCoolDown = 0;
		}
	}
}

void Game::TryPlacePortal(int id) {
	portalPlacementCoolDown = 0;
	// Cast a ray starting at the camera and going forward
	XMFLOAT3 rayOrigin = camera->GetTransform()->GetPosition();
	XMFLOAT3 cameraForward = camera->GetTransform()->GetForward();

	// Convert to XMVECTORs because BoundingBox::Intersects expects XMVECTOR parameters
	XMVECTOR originV = XMLoadFloat3(&rayOrigin);
	XMVECTOR dirV = XMLoadFloat3(&cameraForward);
	XMVECTOR endV = originV + (maxPortalPlacementDistance * dirV);
	dirV = XMVector3Normalize(dirV);

	for (const auto& pair : entities) {
		// Only try to place portals on walls
		if (pair.first.find("wall") == string::npos) {
			continue; 
		}

		Entity* entity = pair.second;
		float hitDist = 0.0f;
		// Check if the ray intersects with the bounding box
		entity->GetBoundingBox().Intersects(originV, dirV, hitDist);
		if (hitDist == 0 || hitDist >= maxPortalPlacementDistance) {
			// Either it didn't intersect, or it intersected too far away.
			continue;
		}
		cout << "Hit " << pair.first << " at distance " << hitDist << endl;

		// Convert ray points from world space to local space of the entity
		XMFLOAT3 startPointLocal = entity->GetTransform()->InverseTransformPoint(rayOrigin);
		XMFLOAT3 endPointLocal;
		XMStoreFloat3(&endPointLocal, endV);
		endPointLocal = entity->GetTransform()->InverseTransformPoint(endPointLocal);
		XMVECTOR startLocalV = XMLoadFloat3(&startPointLocal);
		XMVECTOR endLocalV = XMLoadFloat3(&endPointLocal);

		// Loop through each triangle in the mesh and check for intersection with the ray.
		vector<Vertex> vertices = entity->GetMesh()->GetVertices();
		vector<UINT> indices = entity->GetMesh()->GetIndices();
		XMFLOAT3 closestPoint = XMFLOAT3(FLT_MAX, FLT_MAX, FLT_MAX);
		XMVECTOR normalAtClosestPoint = XMVectorSet(0, 0, 0, 0);
		for (int i = 0; i < indices.size(); i+=3) {
			Vertex v1 = vertices[indices[i]];
			Vertex v2 = vertices[indices[i + 1]];
			Vertex v3 = vertices[indices[i + 2]];
			XMVECTOR p1 = XMLoadFloat3(&v1.Position);
			XMVECTOR p2 = XMLoadFloat3(&v2.Position);
			XMVECTOR p3 = XMLoadFloat3(&v3.Position);
			XMFLOAT3 intersectionPoint;
			// Ray intersect triangle
			if (RayTriangleIntersect(startLocalV, endLocalV - startLocalV, p1, p2, p3, intersectionPoint)) {
				XMFLOAT3 worldPoint = entity->GetTransform()->TransformPoint(intersectionPoint);
				XMVECTOR worldPointV = XMLoadFloat3(&worldPoint);
				XMVECTOR closestPointV = XMLoadFloat3(&closestPoint);
				if (XMVector3LengthSq(worldPointV - originV).m128_f32[0] < XMVector3LengthSq(closestPointV - originV).m128_f32[0]) {
					closestPoint = worldPoint;
					// Calculate the normal at the closest point
					XMVECTOR edge1 = p2 - p1;
					XMVECTOR edge2 = p3 - p1;
					normalAtClosestPoint = XMVector3Cross(edge1, edge2);
					normalAtClosestPoint = XMVector3Normalize(normalAtClosestPoint);
				}
				cout << "World Point: " << worldPoint.x << ", " << worldPoint.y << ", " << worldPoint.z << endl;
			}
		}
		// No triangle intersection found.
		if (closestPoint.x == FLT_MAX) {
			continue;
		}
		cout << "Closest Point: " << closestPoint.x << ", " << closestPoint.y << ", " << closestPoint.z << endl;
		string portalKey = "portal_" + to_string(id);
		// Create portal if it doesn't already exist.
		if (portals.count(portalKey) == 0) {
			XMFLOAT3 color = id == 0 ? XMFLOAT3(0, 0, 1.0f) : XMFLOAT3(1, 0.6f, 0);
			portals.insert({ portalKey, new Portal(meshes[3], materials["portal"], id, color) });
			portals[portalKey]->GetTransform()->SetScale(1, 2, 1);
		}
		string complimentKey = "portal_" + to_string(abs(1 - id));
		if (portals.count(complimentKey) > 0) {
			portals[portalKey]->SetDestination(portals[complimentKey]);
			portals[complimentKey]->SetDestination(portals[portalKey]);
		}
		// Move portal to be flush with the surface, and offset by a small amount to prevent z-fighting.
		XMFLOAT3 newPosition;
		XMStoreFloat3(&newPosition, normalAtClosestPoint * portalOffset);
		newPosition = XMFLOAT3(newPosition.x + closestPoint.x, newPosition.y + closestPoint.y, newPosition.z + closestPoint.z);
		portals[portalKey]->GetTransform()->SetPosition(newPosition.x, newPosition.y, newPosition.z);
		// Rotate the portal to be flush with the surface normal.
		// Note: this only works for placing portals on walls, i.e. only y axis rotation. Placing on the floor/ceiling would require more complex rotation logic.
		XMVECTOR forward = XMVectorSet(0, 0, 1, 0);
		float yRot = XMVectorGetX(XMVector3AngleBetweenVectors(forward, normalAtClosestPoint));
		// If the Cross Product's Y is negative, the angle should be negative
		XMVECTOR cp = XMVector3Cross(forward, normalAtClosestPoint);
		if (XMVectorGetY(cp) < 0) {
			yRot = -yRot;
		}
		cout << "Y Rotation: " << yRot << endl;
		portals[portalKey]->GetTransform()->SetPitchYawRoll(0, yRot, 0);
		if (id == 0) leftPortalTween = 0;
		else rightPortalTween = 0;
	}
}

// Using Moller-Trumbore ray-triangle intersection algorithm. 
// Returns true if the ray intersects the triangle, and outputs the intersection point.
bool Game::RayTriangleIntersect(
	DirectX::XMVECTOR rayOrigin,    // Point 1 of line
	DirectX::XMVECTOR rayDir,       // Direction (Point 2 - Point 1)
	DirectX::XMVECTOR V0,           // Triangle Vertex 0
	DirectX::XMVECTOR V1,           // Triangle Vertex 1
	DirectX::XMVECTOR V2,           // Triangle Vertex 2
	DirectX::XMFLOAT3& outPoint     // The actual intersection coordinate
) {
	using namespace DirectX;

	XMVECTOR edge1 = V1 - V0;
	XMVECTOR edge2 = V2 - V0;
	XMVECTOR pvec = XMVector3Cross(rayDir, edge2);
	XMVECTOR detVec = XMVector3Dot(edge1, pvec);
	float det = XMVectorGetX(detVec);

	// If determinant is near zero, ray is parallel to triangle
	if (det > -0.0001f && det < 0.0001f) return false;

	float invDet = 1.0f / det;
	XMVECTOR tvec = rayOrigin - V0;

	// Calculate U and test bounds
	XMVECTOR uVec = XMVector3Dot(tvec, pvec) * invDet;
	float u = XMVectorGetX(uVec);
	if (u < 0.0f || u > 1.0f) return false;

	XMVECTOR qvec = XMVector3Cross(tvec, edge1);

	// Calculate V and test bounds
	XMVECTOR vVec = XMVector3Dot(rayDir, qvec) * invDet;
	float v = XMVectorGetX(vVec);
	if (v < 0.0f || u + v > 1.0f) return false;

	// Calculate T (distance along the line)
	XMVECTOR tResultVec = XMVector3Dot(edge2, qvec) * invDet;
	float outDistance = XMVectorGetX(tResultVec);

	if (outDistance > 0.0001f) { // Intersection occurs in front of the ray
		XMVECTOR intersectPos = rayOrigin + (rayDir * outDistance);
		XMStoreFloat3(&outPoint, intersectPos);
		return true;
	}

	return false;
}
