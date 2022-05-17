#pragma once
#include <DirectXMath.h>
#include "Transform.h"
#include "Mesh.h"
#include "Camera.h"
#include "Material.h"
#include "Entity.h"
class Portal{
public:
    Portal(Mesh* mesh, Material* mat, int id, XMFLOAT3 borderColor);
    ~Portal();

    DirectX::XMFLOAT4X4 ClippedProjectionMatrix(DirectX::XMFLOAT4X4 viewMat, DirectX::XMFLOAT4X4 projMat);
    Portal* GetDestination();
    void SetDestination(Portal* dest);


    Mesh* GetMesh();
    Transform* GetTransform();
    Material* GetMaterial();
    void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition);
    void UnbindPSAndDraw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition);
    float Sign(float num);
    int GetId();
    XMFLOAT3 GetBorderColor();

private:
    Portal* destination;
    Transform transform;
    Mesh* meshPtr;
    Material* materialPtr;
    int id;
    XMFLOAT3 borderColor;
};
