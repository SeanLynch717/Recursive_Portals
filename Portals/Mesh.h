#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <string>
#include <wrl/client.h> // Used for ComPtr - a smart pointer for COM objects
#include "Vertex.h"
#include <vector>
#include <fstream>

class Mesh {
public:
	Mesh(Vertex vertices[], int number_of_vertices, unsigned int indicies[], int number_of_indicies,
		Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	Mesh(const char* filepath, Microsoft::WRL::ComPtr<ID3D11Device> device, Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);
	~Mesh();
	void CreateBuffers(Vertex vertices[], int number_of_vertices, unsigned int indicies[], int number_of_indicies,
		Microsoft::WRL::ComPtr<ID3D11Device> device);
	void CalculateTangents(Vertex* verts, int numVerts, unsigned int* indices, int numIndices);
	Microsoft::WRL::ComPtr<ID3D11Buffer> GetVertexBuffer();
	Microsoft::WRL::ComPtr<ID3D11Buffer> GetIndexBuffer();
	int GetIndexCount();
	void Draw();

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> index_buffer;
	Microsoft::WRL::ComPtr <ID3D11DeviceContext> context;
	int index_count;

};