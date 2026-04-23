#pragma once
#include <Windows.h>
#include <DirectXMath.h>
#include "Transform.h"

class Camera
{
public:
	Camera(float x, float y, float z, float moveSpeed, float lookSpeed, float fov, float ar, HWND hWnd);
	~Camera();

	// Update methods
	void Update(float dt);
	void UpdateViewMatrix();
	void UpdateProjectionMatrix(float ar);

	// Getters
	DirectX::XMFLOAT4X4 GetView();
	DirectX::XMFLOAT4X4 GetProjection();
	Transform* GetTransform();
	float GetFoV();
	void SetFoV(float fov);

private:
	DirectX::XMFLOAT4X4 viewMatrix;
	DirectX::XMFLOAT4X4 projectionMatrix;

	Transform transform;
	HWND hWnd;
	float movementSpeed;
	float mouseLookSpeed;
	float fieldOfView;
	float aspectRatio;
	bool initialized = false;
};