#include "Transform.h"
using namespace DirectX;

Transform::Transform()
{
    position = XMFLOAT3(0, 0, 0);
    pitchYawRoll = XMFLOAT3(0, 0, 0);
    scale = XMFLOAT3(1, 1, 1);

    XMMATRIX ident = XMMatrixIdentity();
    XMStoreFloat4x4(&worldMatrix, ident);
    XMStoreFloat4x4(&worldInverseTransposeMatrix, ident);

    matricesDirty = false;
}

Transform::~Transform()
{
}

DirectX::XMFLOAT3 Transform::GetPosition()
{
    UpdateMatrices();

    return position;
}

DirectX::XMFLOAT3 Transform::GetPitchYawRoll()
{
    UpdateMatrices();

    return pitchYawRoll;
}

DirectX::XMFLOAT3 Transform::GetScale()
{
    UpdateMatrices();

    return scale;
}

DirectX::XMFLOAT4X4 Transform::GetWorldMatrix()
{
    UpdateMatrices();

    return worldMatrix;
}

DirectX::XMFLOAT4X4 Transform::GetWorldInverseTranspose()
{
    UpdateMatrices();

    return worldInverseTransposeMatrix;
}

DirectX::XMFLOAT3 Transform::GetUp()
{
    // Take the world up vector (0, 1, 0) and rotate it by
    // our orientation values.
    XMVECTOR localUp = XMVector3Rotate(
        XMVectorSet(0, 1, 0, 0),
        XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll))
    );
    XMStoreFloat3(&upVector, localUp);
    return upVector;
}

DirectX::XMFLOAT3 Transform::GetRight()
{
    if(!rotated)
        return rightVector;
    // Take the world right vector (1, 0, 0) and rotate it by
    // our orientation values.
    XMVECTOR localRight = XMVector3Rotate(
        XMVectorSet(1, 0, 0, 0),
        XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll))
    );
    XMStoreFloat3(&rightVector, localRight);
    return rightVector;
}

DirectX::XMFLOAT3 Transform::GetForward()
{
    if(!rotated)
        return forwardVector;
    // Take the world forward vector (0, 0, 1) and rotate it by
    // our orientation values.
    XMVECTOR localForward = XMVector3Rotate(
        XMVectorSet(0, 0, 1, 0),
        XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll))
    );
    XMStoreFloat3(&forwardVector, localForward);
    return forwardVector;
}

void Transform::SetPosition(float x, float y, float z)
{
    position = XMFLOAT3(x, y, z);

    matricesDirty = true;
}

void Transform::SetPitchYawRoll(float pitch, float yaw, float roll)
{
    pitchYawRoll = XMFLOAT3(pitch, yaw, roll);

    matricesDirty = true;
    rotated = true;
}

void Transform::SetScale(float x, float y, float z)
{
    scale = XMFLOAT3(x, y, z);

    matricesDirty = true;
}

void Transform::MoveAbsolute(float x, float y, float z)
{
    position.x += x;
    position.y += y;
    position.z += z;

    matricesDirty = true;
}

void Transform::MoveRelative(float x, float y, float z)
{
    XMVECTOR rotatedVector = XMVector3Rotate(
        XMVectorSet(x, y, z, 0),
        XMQuaternionRotationRollPitchYawFromVector(XMLoadFloat3(&pitchYawRoll)));

    XMStoreFloat3(
        &position,
        XMLoadFloat3(&position) + rotatedVector);
}

void Transform::Rotate(float pitch, float yaw, float roll)
{
    pitchYawRoll.x += pitch;
    pitchYawRoll.y += yaw;
    pitchYawRoll.z += roll;

    matricesDirty = true;
    rotated = true;
}

void Transform::Scale(float x, float y, float z)
{
    scale.x *= x;
    scale.y *= y;
    scale.z *= z;

    matricesDirty = true;
}

void Transform::SetRight(XMFLOAT3 right)
{
    rightVector = right;
    rotated = false;
}

void Transform::SetUp(XMFLOAT3 up)
{
    upVector = up;
    rotated = false;
}

void Transform::SetForward(XMFLOAT3 forward)
{
    forwardVector = forward;
    rotated = false;
}

// Convert from local space to world space.
DirectX::XMFLOAT3 Transform::TransformPoint(DirectX::XMFLOAT3 point)
{
    XMMATRIX rotMatrix = XMMatrixRotationQuaternion(XMQuaternionRotationRollPitchYaw(pitchYawRoll.x, pitchYawRoll.y, pitchYawRoll.z));
    XMMATRIX scaleMatrix = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMFLOAT3 newPos;
    XMStoreFloat3(
        &newPos, 
        XMVector4Transform(
            XMVector4Transform(
                XMVectorSet(point.x, point.y, point.z, 0), 
                scaleMatrix), 
            rotMatrix) 
        + XMVectorSet(position.x, position.y, position.z, 0)
    );
    return newPos;
}

// Convert from world space to local space.
DirectX::XMFLOAT3 Transform::InverseTransformPoint(DirectX::XMFLOAT3 point)
{
    XMVECTOR pointV = XMLoadFloat3(&point);
    XMVECTOR positionV = XMLoadFloat3(&position);
    XMVECTOR sub = XMVectorSubtract(pointV, positionV);
    XMMATRIX inverseRotationMatrix = XMMatrixRotationQuaternion(XMQuaternionInverse(XMQuaternionRotationRollPitchYaw(pitchYawRoll.x, pitchYawRoll.y, pitchYawRoll.z)));
    XMMATRIX scaleMatrix = XMMatrixScaling(1 / scale.x, 1 / scale.y, 1 / scale.z);

    XMFLOAT3 retVal;
    XMStoreFloat3(
        &retVal,
        XMVector4Transform(
            XMVector4Transform(
                sub,
                inverseRotationMatrix
            ),
            scaleMatrix
        )
    );
    return retVal;
}

void Transform::UpdateMatrices()
{
    // Do we actually need to update anything?
    if (!matricesDirty)
        return;

    // Actually update the matrices by creating the
    // individual transformation matrices and combining
    XMMATRIX transMat = XMMatrixTranslation(position.x, position.y, position.z);
    XMMATRIX rotMat = XMMatrixRotationRollPitchYaw(pitchYawRoll.x, pitchYawRoll.y, pitchYawRoll.z);
    XMMATRIX scaleMat = XMMatrixScaling(scale.x, scale.y, scale.z);

    // Combine into a single matrix that represents all transformations
    // and store the results
    XMMATRIX worldMat = scaleMat * rotMat * transMat; // S-R-T
    XMStoreFloat4x4(&worldMatrix, worldMat);

    // While we're at it, might as well create the inverse transpose matrix, too
    XMStoreFloat4x4(
        &worldInverseTransposeMatrix,
        XMMatrixInverse(0, XMMatrixTranspose(worldMat)));


    // We're clean again
    matricesDirty = false;
}
