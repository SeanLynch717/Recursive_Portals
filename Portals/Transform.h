#pragma once
#include <DirectXMath.h>
class Transform
{
public:
    Transform();
    ~Transform();

    // Getters
    DirectX::XMFLOAT3 GetPosition();
    DirectX::XMFLOAT3 GetPitchYawRoll();
    DirectX::XMFLOAT3 GetScale();
    DirectX::XMFLOAT4X4 GetWorldMatrix();
    DirectX::XMFLOAT4X4 GetWorldInverseTranspose();

    DirectX::XMFLOAT3 GetUp();
    DirectX::XMFLOAT3 GetRight();
    DirectX::XMFLOAT3 GetForward();


    // Setters
    void SetPosition(float x, float y, float z);
    void SetPitchYawRoll(float pitch, float yaw, float roll);
    void SetScale(float x, float y, float z);

    // Transformers
    void MoveAbsolute(float x, float y, float z);    // world
    void MoveRelative(float x, float y, float z); // local
    void Rotate(float pitch, float yaw, float roll);
    void Scale(float x, float y, float z);
    void SetRight(DirectX::XMFLOAT3 right);
    void SetUp(DirectX::XMFLOAT3 up);
    void SetForward(DirectX::XMFLOAT3 forward);

    DirectX::XMFLOAT3 TransformPoint(DirectX::XMFLOAT3 point);
    DirectX::XMFLOAT3 InverseTransformPoint(DirectX::XMFLOAT3 point);

private:
    // Raw transformation data
    DirectX::XMFLOAT3 position;    // default 0,0,0
    DirectX::XMFLOAT3 pitchYawRoll;    // default 0,0,0
    DirectX::XMFLOAT3 scale;    // default 1,1,1

    DirectX::XMFLOAT3 rightVector;
    DirectX::XMFLOAT3 upVector;
    DirectX::XMFLOAT3 forwardVector;

    // Matrices
    bool matricesDirty;
    DirectX::XMFLOAT4X4 worldMatrix;
    DirectX::XMFLOAT4X4 worldInverseTransposeMatrix;

    bool rotated;

    // Helper for updating matrices
    void UpdateMatrices();
};
