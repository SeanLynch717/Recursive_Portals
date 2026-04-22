#pragma once

#include "DXCore.h"
#include "Mesh.h"

#define PI 3.14159265

using namespace std;

class MeshFactory: public DXCore {
public:
	MeshFactory() = delete;

	static Mesh* CreateCircleMesh(int numSlices, Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
};