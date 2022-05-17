#include "Game.h"
#include "Vertex.h"
#include "Input.h"
#include "WICTextureLoader.h"
// Needed for a helper function to read compiled shader files from the hard drive
#pragma comment(lib, "d3dcompiler.lib")
#include <d3dcompiler.h>
#include <math.h>
#include <iostream>

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
	for (Entity* entity : entities) {
		delete entity;
	}
	for (Material* material : materials) {
		delete material;
	}

	delete camera;
	delete vertexShader;
	delete portalPixelShader;
	delete lightingPixelShader;
	delete skyVS;
	delete skyPS;
	delete skyBox;
	delete portals[0];
	delete portals[1];
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
	// Create the skybox
	//skyBox = new Sky(GetFullPathTo_Wide(L"../../Assets/Textures/Skies/SunnyCubeMap.dds").c_str(), skyVS, skyPS, meshes[0], sampler, device, context);

	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create the camera once we have the aspect ratio
	camera = new Camera(-10, 2, 5, 5.0f, .5f, XM_PIDIV4, (float)width / height);
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

	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP; // What happens outside the 0-1 uv range?
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;		// How do we handle sampling "between" pixels?
	sampDesc.MaxAnisotropy = 16;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	device->CreateSamplerState(&sampDesc, sampler.GetAddressOf());

	// Cobblestone Material
	materials.push_back(new Material(XMFLOAT4(1, 1, 1, 0), lightingPixelShader, vertexShader, 0.0f));
	materials[0]->AddSampler("BasicSampler", sampler);
	materials[0]->AddTextureSRV("Albedo", cobblestoneAlbedoRSV);
	materials[0]->AddTextureSRV("RoughnessMap", cobblestoneSpecularRSV);
	materials[0]->AddTextureSRV("NormalMap", cobblestoneNormalRSV);
	materials[0]->AddTextureSRV("MetalnessMap", cobblestoneMetalnessRSV);

	// Floor Material
	materials.push_back(new Material(XMFLOAT4(1, 1, 1, 0), lightingPixelShader, vertexShader, 0.0f));
	materials[1]->AddSampler("BasicSampler", sampler);
	materials[1]->AddTextureSRV("Albedo", floorAlbedoRSV);
	materials[1]->AddTextureSRV("RoughnessMap", floorSpecularRSV);
	materials[1]->AddTextureSRV("NormalMap", floorNormalRSV);
	materials[1]->AddTextureSRV("MetalnessMap", floorMetalnessRSV);

	// Metal Material
	materials.push_back(new Material(XMFLOAT4(1, 1, 1, 0), lightingPixelShader, vertexShader, 0.0f));
	materials[2]->AddSampler("BasicSampler", sampler);
	materials[2]->AddTextureSRV("Albedo", metalAlbedoRSV);
	materials[2]->AddTextureSRV("RoughnessMap", metalSpecularRSV);
	materials[2]->AddTextureSRV("NormalMap", metalNormalRSV);
	materials[2]->AddTextureSRV("MetalnessMap", metalMetalnessRSV);

	// Portal Material
	materials.push_back(new Material(XMFLOAT4(1, 1, 1, 0), portalPixelShader, vertexShader, 0.0f));
	materials[3]->AddSampler("BasicSampler", sampler);
}

// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{
	Mesh* mesh1 = new Mesh(GetFullPathTo("../../Assets/Models/cube.obj").c_str(), device, context);
	meshes.push_back(mesh1);
	Mesh* mesh2 = new Mesh(GetFullPathTo("../../Assets/Models/sphere.obj").c_str(), device, context);
	meshes.push_back(mesh2);
	Mesh* portalMesh = new Mesh(GetFullPathTo("../../Assets/Models/quad.obj").c_str(), device, context);
	meshes.push_back(portalMesh);

	// Create scene mesh.
	entities.push_back(new Entity(meshes[0], materials[0]));
	entities[0]->GetTransform()->SetScale(20, .01f, 10);
	entities[0]->GetTransform()->MoveAbsolute(0, 0, 5);
	entities.push_back(new Entity(meshes[0], materials[0]));
	entities[1]->GetTransform()->SetScale(20, 20, .01f);
	entities[1]->GetTransform()->SetPosition(0, 10, 10);
	entities[1]->GetTransform()->SetPitchYawRoll(0, 0, PI / 2);
	entities.push_back(new Entity(meshes[0], materials[0]));
	entities[2]->GetTransform()->SetScale(20, 20, .01f);
	entities[2]->GetTransform()->SetPosition(0, 10, 0);
	entities.push_back(new Entity(meshes[1], materials[2]));
	entities[3]->GetTransform()->SetScale(1, 1, 1);
	entities[3]->GetTransform()->MoveAbsolute(0, 2, 5);
	//entities.push_back(new Entity(meshes[0], materials[0]));
	//entities[4]->GetTransform()->SetScale(20, .01f, 10);
	//entities[4]->GetTransform()->MoveAbsolute(0, 4, 5);
	//entities[4]->GetTransform()->SetPitchYawRoll(PI, 0, 0);

	// For debugging the virtual cameras position.
	//virtualCamera[0] = new Entity(meshes[0], materials[0]);
	//virtualCamera[0]->GetTransform()->MoveAbsolute(0, 0, 0);
	//virtualCamera[1] = new Entity(meshes[0], materials[0]);
	//virtualCamera[1]->GetTransform()->MoveAbsolute(0, 0, 0);
	
	// First set of portals.
	portals[0] = new Portal(meshes[2], materials[3], 0, XMFLOAT3(0, 0, 1));
	portals[0]->GetTransform()->MoveAbsolute(0, 2, 10);
	portals[0]->GetTransform()->SetPitchYawRoll(0, PI, 0);
	portals[0]->GetTransform()->SetScale(1, 2, 1);
	portals[1] = new Portal(meshes[2], materials[3], 1, XMFLOAT3(0, 0, 1));
	portals[1]->GetTransform()->MoveAbsolute(10, 2, 8);
	portals[1]->GetTransform()->SetPitchYawRoll(0, -PI/2, 0);
	portals[1]->GetTransform()->SetScale(1, 2, 1);
	portals[0]->SetDestination(portals[1]);
	portals[1]->SetDestination(portals[0]);

	// Second set of portals
	portals[2] = new Portal(meshes[2], materials[3], 0, XMFLOAT3(1, 0.6f, 0));
	portals[2]->GetTransform()->MoveAbsolute(-4, 2, 10);
	portals[2]->GetTransform()->SetPitchYawRoll(0, PI, 0);
	portals[2]->GetTransform()->SetScale(1, 2, 1);
	portals[3] = new Portal(meshes[2], materials[3], 1, XMFLOAT3(1, 0.6f, 0));
	portals[3]->GetTransform()->MoveAbsolute(-4, 2, 0);
	portals[3]->GetTransform()->SetPitchYawRoll(0, 0, 0);
	portals[3]->GetTransform()->SetScale(1, 2, 1);
	portals[2]->SetDestination(portals[3]);
	portals[3]->SetDestination(portals[2]);

	// Third set of portals
	portals[4] = new Portal(meshes[2], materials[3], 0, XMFLOAT3(0, 1, 0));
	portals[4]->GetTransform()->MoveAbsolute(10, 2, 3);
	portals[4]->GetTransform()->SetPitchYawRoll(0, -PI/2, 0);
	portals[4]->GetTransform()->SetScale(1, 2, 1);
	portals[5] = new Portal(meshes[2], materials[3], 1, XMFLOAT3(0, 1, 0));
	portals[5]->GetTransform()->MoveAbsolute(-10, 2, 3);
	portals[5]->GetTransform()->SetPitchYawRoll(0, PI/2, 0);
	portals[5]->GetTransform()->SetScale(1, 2, 1);
	portals[4]->SetDestination(portals[5]);
	portals[5]->SetDestination(portals[4]);

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
		portalAnimation = !portalAnimation;
	}
	//if (Input::GetInstance().KeyPress('B')) {
	//	drawSkyBox = !drawSkyBox;
	//}
	CheckPortalCollision();
	portalCoolDown += deltaTime;
	camera->Update(deltaTime);
}

void Game::UpdateTransforms(float deltaTime, float totalTime)
{
	XMFLOAT3 pos = entities[3]->GetTransform()->GetPosition();
	entities[3]->GetTransform()->SetPosition(2 * sin(totalTime), pos.y, pos.z);
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

	for (Portal* p : portals) {
		SimplePixelShader* ps = p->GetMaterial()->GetPixelShader();
		if (portalAnimation) {
			ps->SetFloat("totalTime", totalTime);
		}
		else {
			ps->SetFloat("totalTime", 0);
		}
	}
	// Draw Portals
	DrawPortals(camera->GetView(), camera->GetProjection(), camera->GetTransform()->GetPosition(), 1, 0);
	
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
	for (int i = 0; i < entities.size(); i++) {
		if (!drawWalls && (i == 1 || i == 2)) continue;
		Entity* entity = entities[i];
		SimplePixelShader* ps = entity->GetMaterial()->GetPixelShader();
		ps->SetData("lights", &lights[0], sizeof(Light) * (int)lights.size());
		ps->SetFloat3("ambientColor", ambientColor);
		entity->Draw(context, viewMat, projMat, cameraPosition);
	}
}

// Draw the portals by calculating the virtual cameras view and clipped projection matrix, and using the stencil buffer.
// This method is currently not recursive, but was set up in such a way to support recursive portals down the line.
void Game::DrawPortals(XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition, int maxRecursion, int recursionLevel)
{
	//Portal* portal = portals[2];
	for (auto& portal : portals) {
		// Set Depth Stencil State
		context->OMSetDepthStencilState(stencilWriteMask.Get(), recursionLevel);	
		portal->UnbindPSAndDraw(context, viewMat, projMat, cameraPosition);

		// Calculate the view from the perspective of the out portal
		XMMATRIX rotationMatrix = XMMatrixRotationAxis(XMVectorSet(0, 1, 0, 0), PI);
		XMMATRIX V = XMMatrixInverse(0, XMLoadFloat4x4(&portal->GetDestination()->GetTransform()->GetWorldMatrix()))
			* rotationMatrix
			* XMLoadFloat4x4(&portal->GetTransform()->GetWorldMatrix())
			* XMLoadFloat4x4(&viewMat);
		XMFLOAT4X4 viewDest;
		XMStoreFloat4x4(&viewDest, V);

		// Calculate Oblique Clipped Projection Matrix. Use standard projection matrix for now.
		XMFLOAT4X4 newProj = projMat; //portal->GetDestination()->ClippedProjectionMatrix(viewDest, projMat);

		// Calculate new camera position
		XMFLOAT3 relPos = portal->GetTransform()->InverseTransformPoint(camera->GetTransform()->GetPosition());
		XMStoreFloat3(
			&relPos,
			XMVector3Transform(XMLoadFloat3(&relPos), rotationMatrix)
		);
		relPos = portal->GetDestination()->GetTransform()->TransformPoint(relPos);

		// Base case: 
		if (recursionLevel == maxRecursion) {
			// Set depth stencil state
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
		// Draw portal into stencil buffer. This will undo the stencil bits we incremented, 
		// eventually returning to a buffer full of zeroes.a
		portal->UnbindPSAndDraw(context, viewMat, projMat, cameraPosition);
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
	for (auto& portal : portals) {
		portal->UnbindPSAndDraw(context, viewMat, projMat, cameraPosition);
	}
	
	// Set depth stencil state
	context->OMSetDepthStencilState(portalBorderMask.Get(), recursionLevel);

	// Draw portal outline
	for (auto& portal : portals) {
		portal->GetMaterial()->GetPixelShader()->SetInt("drawRecursive", 1);
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

	for (auto& portal : portals) {
		// Calculate which side of the plane the camera is on.
		XMFLOAT3 portalPos = portal->GetTransform()->GetPosition();
		XMFLOAT3 diff = XMFLOAT3(cameraPos.x - portalPos.x, cameraPos.y - portalPos.y, cameraPos.z - portalPos.z);
		float dot = XMVector3Dot(XMLoadFloat3(&diff), XMLoadFloat3(&portal->GetTransform()->GetForward())).m128_f32[0];
		// Calculate if the camera falls within the dimensions of the portal.
		XMFLOAT3 right = portal->GetTransform()->GetRight();
		XMFLOAT3 up = portal->GetTransform()->GetUp();
		float rightDot = (right.x * diff.x) + (right.y * diff.y) + (right.z * diff.z);
		float upDot = (up.x * diff.x) + (up.y * diff.y) + (up.z * diff.z);
		XMFLOAT3 planeProj = XMFLOAT3((right.x * rightDot) + (up.x * upDot), (right.y * rightDot) + (up.y * upDot), (right.z * rightDot) + (up.z * upDot));
		float dist = sqrt(pow(planeProj.x, 2) + pow(planeProj.y, 2) + pow(planeProj.z, 2));

		// Check if the camera is on the negative side of the plane, AND the magnitude of the cameras projection 
		// onto the portal plane is within bounds of the portals frame.
		if (dot <= 0.5f && dist <= 1) {
			XMFLOAT3 destPos = portal->GetDestination()->GetTransform()->GetPosition();
			XMFLOAT3 portalRot = portal->GetTransform()->GetPitchYawRoll();
			XMFLOAT3 destRot = portal->GetDestination()->GetTransform()->GetPitchYawRoll();
			XMFLOAT3 cameraRot = camera->GetTransform()->GetPitchYawRoll();
			float rotDiff = cameraRot.y + PI - portalRot.y;
			XMVECTOR xmDiff = XMLoadFloat3(&diff);
			XMFLOAT3 localDiff;
			XMStoreFloat3(&localDiff,
				XMVectorSet(
					XMVector3Dot(xmDiff, XMLoadFloat3(&portal->GetDestination()->GetTransform()->GetRight())).m128_f32[0],
					XMVector3Dot(xmDiff, XMLoadFloat3(&portal->GetDestination()->GetTransform()->GetUp())).m128_f32[0],
					XMVector3Dot(xmDiff, XMLoadFloat3(&portal->GetDestination()->GetTransform()->GetForward())).m128_f32[0],
					0
				)
			);
			camera->GetTransform()->SetPosition(destPos.x - localDiff.x, destPos.y + localDiff.y, destPos.z - localDiff.z);
			camera->GetTransform()->SetPitchYawRoll(cameraRot.x, destRot.y + rotDiff, cameraRot.z);
			portalCoolDown = 0;
		}
	}
}
