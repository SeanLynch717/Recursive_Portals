#include "Camera.h"
#include "Input.h"
#include <string>
#include <iostream>
#define PI 3.14159265
using namespace DirectX;
using namespace std;

Camera::Camera(float x, float y, float z, float moveSpeed, float lookSpeed, float fov, float ar, HWND hWnd) :
	movementSpeed(moveSpeed),
	mouseLookSpeed(lookSpeed),
	fieldOfView(fov),
	aspectRatio(ar),
	hWnd(hWnd)
{
	// setup transform
	transform.SetPosition(x, y, z);
	UpdateViewMatrix();
	UpdateProjectionMatrix(ar);
}

Camera::~Camera()
{
}

void Camera::Update(float dt)
{
	// Get a reference to the input manager
	Input& input = Input::GetInstance();

	//Calculate current speed
	float speed = movementSpeed * dt;

	// Sprinting
	if (input.KeyDown(VK_CONTROL)) speed *= 3;

	// Movement
	if (input.KeyDown('W')) { transform.MoveRelative(0, 0, speed); }
	if (input.KeyDown('S')) { transform.MoveRelative(0, 0, -speed); }
	if (input.KeyDown('A')) { transform.MoveRelative(-speed, 0, 0); }
	if (input.KeyDown('D')) { transform.MoveRelative(speed, 0, 0); }
	if (input.KeyDown(VK_SHIFT)) { transform.MoveAbsolute(0, -speed, 0); }
	if (input.KeyDown(VK_SPACE)) { transform.MoveAbsolute(0, speed, 0); }

	// Control rotation of the camera
	// Get the center of the window
	RECT rect;
	GetClientRect(hWnd, &rect);
	POINT center = { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };

	// Get current mouse position (relative to window)
	POINT current;
	GetCursorPos(&current);
	ScreenToClient(hWnd, &current);

	if (mouseEnabled) {
		// Calculate how far it moved from center
		float dx = dt * mouseLookSpeed * ((float)current.x - center.x);
		float dy = dt * mouseLookSpeed * ((float)current.y - center.y);

		if (dx != 0 || dy != 0)
		{
			if (initialized == true) {
				// Rotate the transform!SWAP X AND Y!
				this->transform.Rotate(dy, dx, 0);

				DirectX::XMFLOAT3 rotation = transform.GetPitchYawRoll();
				float epsilon = .01f;
				if (rotation.x > PI / 2 - epsilon) transform.SetPitchYawRoll(PI / 2 - epsilon, rotation.y, rotation.z);
				if (rotation.x < -PI / 2 + epsilon) transform.SetPitchYawRoll(-PI / 2 + epsilon, rotation.y, rotation.z);
			}
			initialized = true;
			// Force the cursor back to the center
			POINT screenCenter = center;
			ClientToScreen(hWnd, &screenCenter);
			SetCursorPos(screenCenter.x, screenCenter.y);
		}
	}

	// At the end, update the view
	UpdateViewMatrix();
}

void Camera::UpdateViewMatrix()
{
	XMFLOAT3 pos = transform.GetPosition();
	XMFLOAT3 fwd = transform.GetForward();

	XMMATRIX V = XMMatrixLookToLH(
		XMLoadFloat3(&pos),			// Camera's position
		XMLoadFloat3(&fwd),			// Camera's forward vector
		XMVectorSet(0, 1, 0, 0));   // World up (Y)

	XMStoreFloat4x4(&viewMatrix, V);
}

void Camera::UpdateProjectionMatrix(float ar)
{
	aspectRatio = ar;
	XMMATRIX P = XMMatrixPerspectiveFovLH(fieldOfView, ar, 0.01f, 100.0f);
	XMStoreFloat4x4(&projectionMatrix, P);
}

DirectX::XMFLOAT4X4 Camera::GetView()
{
	return viewMatrix;
}

DirectX::XMFLOAT4X4 Camera::GetProjection()
{
	return projectionMatrix;
}

Transform* Camera::GetTransform()
{
	return &transform;
}

float Camera::GetFoV()
{
	return fieldOfView;
}

void Camera::SetFoV(float fov)
{
	fieldOfView = fov;
	UpdateProjectionMatrix(aspectRatio);
}

void Camera::ToggleMouse()
{
	mouseEnabled = !mouseEnabled;
}