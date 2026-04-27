#include "Entity.h"
using namespace DirectX;
Entity::Entity(Mesh* mesh, Material* material) {
	meshPtr = mesh;
	materialPtr = material;
	transform = Transform();
	// Calculate AABB in world space
}
Entity::~Entity() {

}
Mesh* Entity::GetMesh() {
	return meshPtr;
}
Transform* Entity::GetTransform() {
	return &transform;
}

Material* Entity::GetMaterial()
{
	return materialPtr;
}

BoundingBox Entity::GetBoundingBox() {
	if (transform.boundsDirty) {
		XMFLOAT3 localMin = meshPtr->GetLocalMin();
		XMFLOAT3 localMax = meshPtr->GetLocalMax();
		XMFLOAT3 worldCorner1 = transform.TransformPoint(XMFLOAT3(localMin.x, localMin.y, localMin.z));
		XMFLOAT3 worldCorner2 = transform.TransformPoint(XMFLOAT3(localMax.x, localMax.y, localMax.z));
		XMFLOAT3 extents = XMFLOAT3(
			abs(worldCorner2.x - worldCorner1.x) / 2,
			abs(worldCorner2.y - worldCorner1.y) / 2,
			abs(worldCorner2.z - worldCorner1.z) / 2
		);
		boundingBox = BoundingBox(transform.GetPosition(), extents);
		transform.boundsDirty = false;
	}
	return boundingBox;
}

void Entity::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, XMFLOAT4X4 viewMat, XMFLOAT4X4 projMat, XMFLOAT3 cameraPosition)
{
	materialPtr->PrepareMaterial(&transform, viewMat, projMat, cameraPosition);

	//context->VSSetConstantBuffers(
	//	0,  // Which slot (register) to bind the buffer to?
	//	1,  // How many are we activating?  Can do multiple at once 
	//	constantBufferVS.GetAddressOf());  // Array of buffers (or the address of one)

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, meshPtr->GetVertexBuffer().GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(meshPtr->GetIndexBuffer().Get(), DXGI_FORMAT_R32_UINT, 0);


	// Finally do the actual drawing
	//  - Do this ONCE PER OBJECT you intend to draw
	//  - This will use all of the currently set DirectX "stuff" (shaders, buffers, etc)
	//  - DrawIndexed() uses the currently set INDEX BUFFER to look up corresponding
	//     vertices in the currently set VERTEX BUFFER
	context->DrawIndexed(
		meshPtr->GetIndexCount(),     // The number of indices to use (we could draw a subset if we wanted)
		0,     // Offset to the first index we want to use
		0);    // Offset to add to each index when looking up vertices
}