#include "Portal.h"
#include <math.h>
#include <iostream>
using namespace DirectX;

Portal::Portal(Mesh* mesh, Material* mat, int id, XMFLOAT3 borderColor)
{
	meshPtr = mesh;
	materialPtr = mat;
	transform = Transform();
	this->id = id;
	this->borderColor = borderColor;
}

Portal::~Portal()
{
}

void Portal::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition) {
	materialPtr->PrepareMaterial(&transform, viewMat, projMat, cameraPosition);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, meshPtr->GetVertexBuffer().GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(meshPtr->GetIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);

	context->DrawIndexed(
		meshPtr->GetIndexCount(),     // The number of indices to use (we could draw a subset if we wanted)
		0,     // Offset to the first index we want to use
		0);    // Offset to add to each index when looking up vertices
}
void Portal::UnbindPSAndDraw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition)
{
	// Unbind pixel shader
	SimplePixelShader* shader = this->materialPtr->GetPixelShader();
	this->GetMaterial()->SetPixelShader(NULL);
	context->PSSetShader(NULL, NULL, 0);
	materialPtr->PrepareMaterial(&transform, viewMat, projMat, cameraPosition);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, meshPtr->GetVertexBuffer().GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(meshPtr->GetIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);
	context->DrawIndexed(
		meshPtr->GetIndexCount(),     // The number of indices to use (we could draw a subset if we wanted)
		0,     // Offset to the first index we want to use
		0);    // Offset to add to each index when looking up vertices

	// Rebind pixel shader
	this->GetMaterial()->SetPixelShader(shader);
}
Mesh* Portal::GetMesh() {
	return meshPtr;
}
Transform* Portal::GetTransform() {
	return &transform;
}

Material* Portal::GetMaterial()
{
	return materialPtr;
}

// Set the near plane to be mesh with the surface of the portal, while adjusting remaining sides for optimal viewing frustum.
// http://www.terathon.com/lengyel/Lengyel-Oblique.pdf
// http://www.terathon.com/code/oblique.html
// http://perry.cz/articles/ProjectionMatrix.xhtml
// https://gamedev.net/forums/topic/398719-oblique-frustum-clipping/3644560/
DirectX::XMFLOAT4X4 Portal::ClippedProjectionMatrix(XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat)
{
    Transform* t = this->GetTransform();
    XMFLOAT3 pos = t->GetPosition();
    XMVECTOR worldPos = XMLoadFloat3(&pos);
    XMFLOAT3 rot = t->GetPitchYawRoll();

    // 1. Calculate the World Space Normal of the portal
    // We assume the portal's "forward" face is the Z-axis (0, 0, 1)
    XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
    XMVECTOR worldNormal = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), rotationMatrix);

    // 2. Transform the Portal Position and Normal into View Space
    XMMATRIX mViewMat = XMLoadFloat4x4(&viewMat);
    XMVECTOR viewPos = XMVector3TransformCoord(worldPos, mViewMat);
    XMVECTOR viewNormal = XMVector3TransformNormal(worldNormal, mViewMat);

    // Normalize to be safe
    viewNormal = XMVector3Normalize(viewNormal);

    // 3. Construct the Plane Equation in View Space: Ax + By + Cz + w = 0
    // w is the negative dot product of the normal and a point on the plane
    float w = -XMVectorGetX(XMVector3Dot(viewNormal, viewPos));

    // Small epsilon to prevent "Z-fighting" or accidental clipping of the portal surface itself
    w -= 0.01f;

    XMVECTOR xmClipPlane = XMVectorSet(XMVectorGetX(viewNormal), XMVectorGetY(viewNormal), XMVectorGetZ(viewNormal), w);

    // 4. Calculate the "q" vector (the far-corner of the frustum opposite the clipping plane)
    // This is used to scale the clip plane so it replaces the near plane perfectly.
    // Note: projMat._31, _32, etc. assumes standard DirectX column-major access.
    XMVECTOR q = XMVectorSet(
        (Sign(XMVectorGetX(xmClipPlane)) + projMat._31) / projMat._11,
        (Sign(XMVectorGetY(xmClipPlane)) + projMat._32) / projMat._22,
        1.0f,
        (1.0f + projMat._33) / projMat._43
    );

    // 5. Scale the clip plane
    XMVECTOR xmDot = XMVector4Dot(xmClipPlane, q);
    XMVECTOR scaledPlane = xmClipPlane * (2.0f / XMVectorGetX(xmDot));

    // 6. Replace the third column of the projection matrix with our new plane
    XMFLOAT4 c;
    XMStoreFloat4(&c, scaledPlane);

    XMFLOAT4X4 newProj = projMat;
    newProj._13 = c.x;
    newProj._23 = c.y;
    newProj._33 = c.z;
    newProj._43 = c.w;

    return newProj;
}

// Old, broken version of the oblique clipped projection matrix calculation.
//DirectX::XMFLOAT4X4 Portal::OLDClippedProjectionMatrix(XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat)
//{
//	Transform* t = this->GetTransform();
//	XMFLOAT3 pos = t->GetPosition();
//	XMFLOAT3 rot = t->GetPitchYawRoll();
//
//	// Calculate distance from portal to the origin.
//	float distance = sqrt(pow(pos.x, 2) + pow(pos.y, 2) + pow(pos.z, 2)) - 1.29;// Offset the distance from the origin to the portal just enough so that the plane the portal is on is truncated.
//
//	// Convert the float4x4 view mat to a XMMatrix
//	XMMATRIX mViewMat = XMLoadFloat4x4(&viewMat);
//	// Convert the float4x4 projection matrix to a XMMatrix
//	XMMATRIX mProjMat = XMLoadFloat4x4(&projMat);
//	// Convert orientation to a quaternion.
//	XMVECTOR quat = XMQuaternionRotationRollPitchYaw(rot.x, rot.y, rot.z);
//	// Negative 'e3' standard basis vector.
//	XMVECTOR negZ = XMVectorSet(0, 0, 1.0f, 0);
//	// Calculate the clip plane.
//	XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
//	XMVECTOR xmClipPlane = XMVector4Transform(negZ, rotationMatrix);	
//	float dot2 = XMVector4Dot(xmClipPlane, XMLoadFloat3(&pos)).m128_f32[0];
//	xmClipPlane = XMVectorSetByIndex(xmClipPlane, distance, 3);
//	xmClipPlane = XMVector4Transform(xmClipPlane, XMMatrixInverse(0, XMMatrixTranspose(mViewMat)));
//
//	// Convert clip plane to a float 4.
//	XMFLOAT4 clipPlane;
//	XMStoreFloat4(&clipPlane, xmClipPlane);
//
//	XMVECTOR q = XMVectorSet((Sign(clipPlane.x) - projMat._31) / projMat._11, (Sign(clipPlane.y) - projMat._32) / projMat._22, 1.0f, (1.0f - projMat._33) / projMat._43);
//	XMVECTOR xmDot = XMVector4Dot(xmClipPlane, q);
//	float dot = xmDot.m128_f32[0];
//	XMVECTOR scaledPlane = xmClipPlane * (1.0f / dot);
//	XMFLOAT4 c;
//	XMStoreFloat4(&c, scaledPlane);
//
//	// Replace 3rd column of the projection matrix to 
//	XMFLOAT4X4 newProj = projMat;
//	newProj._13 = c.x;
//	newProj._23 = c.y;
//	newProj._33 = c.z;
//	newProj._43 = c.w;
//
//	return newProj;
//}

Portal* Portal::GetDestination()
{
	return destination;
}

void Portal::SetDestination(Portal* dest)
{ 
	destination = dest;
}

float Portal::Sign(float num)
{
	if (num > 0) return 1;
	else if (num < 0) return -1;
	return 0;
}

int Portal::GetId()
{
	return id;
}

XMFLOAT3 Portal::GetBorderColor()
{
	return borderColor;
}
