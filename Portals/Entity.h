#pragma once
#include "Transform.h"
#include "Mesh.h"
#include "Camera.h"
#include "Material.h"
#include <DirectXCollision.h>

class Entity {
public:
	Entity(Mesh* meshPtr, Material* material);
	~Entity();

	Mesh* GetMesh();
	Transform* GetTransform();
	Material* GetMaterial();
	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition);
	DirectX::BoundingBox GetBoundingBox();
private:
	Transform transform;
	Mesh* meshPtr;
	Material* materialPtr;
	BoundingBox boundingBox;
};
