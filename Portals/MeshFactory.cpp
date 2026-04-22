#include "MeshFactory.h"
#include <DirectXMath.h>
#include <iostream>

using namespace DirectX;


Mesh* MeshFactory::CreateCircleMesh(int numSlices, Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context) {
	// Create circle mesh
	Vertex* verts = new Vertex[numSlices + 1];
	unsigned int* indices = new unsigned int[numSlices * 3];
	float dAngle = 2 * PI / numSlices;
	float angle = 0;
	// Create center of circle
	Vertex origin;
	origin.Position = XMFLOAT3(0, 0, 0);
	origin.UV = XMFLOAT2(0.5f, 0.5f);
	origin.Normal = XMFLOAT3(0, 0, 1.0f);
	verts[0] = origin;
	// Construct points in circle, and add triangles to indices array
	for (int i = 1; i <= numSlices; i++) {
		int i1 = 0;
		int i2 = i;
		int i3 = i + 1;
		if (i3 > numSlices) {
			i3 = 1;
		}
		// Create vertex and add to verts array
		float x = cos(angle);
		float y = sin(angle);
		verts[i].Position = XMFLOAT3(x, y, 0);
		verts[i].UV = XMFLOAT2((x + 1.0f) / 2.0f, (1.0f - y) / 2.0f);
		verts[i].Normal = XMFLOAT3(0, 0, 1.0f);

		// Add triangle starting at this vert to indices array
		indices[(i - 1) * 3] = i1;
		indices[((i - 1) * 3) + 1] = i2;
		indices[((i - 1) * 3) + 2] = i3;
		// Increment angle
		angle += dAngle;
	}
	Mesh* circleMesh = new Mesh(verts, numSlices + 1, indices, numSlices * 3, device, context);
	delete[] verts;
	delete[] indices;
	return circleMesh;
}