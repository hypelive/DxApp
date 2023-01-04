#include <algorithm>

#include "Camera.h"


Camera::Camera(const XMFLOAT3 position)
{
	m_position = position;
	m_pitch = 0.0f;
	m_yaw = -XM_PIDIV2;

	UpdateOrientation();
}


void Camera::Translate(const XMFLOAT3 offset)
{
	const XMVECTOR positionVec = XMLoadFloat3(&m_position);
	const XMVECTOR rightVec = XMLoadFloat3(&m_right);
	const XMVECTOR upVec = XMLoadFloat3(&m_up);
	const XMVECTOR forwardVec = XMLoadFloat3(&m_forward);

	XMStoreFloat3(&m_position, positionVec + offset.x * rightVec + offset.y * upVec + offset.z * forwardVec);
}


void Camera::Rotate(const XMFLOAT2 angles)
{
	m_yaw += angles.x;
	if (m_yaw > XM_PI)
		m_yaw -= XM_2PI;
	if (m_yaw <= -XM_PI)
		m_yaw += XM_2PI;

	m_pitch = std::clamp(m_pitch + angles.y, -XM_PIDIV2 + kEpsilon, XM_PIDIV2 - kEpsilon);

	UpdateOrientation();
}


// TODO LH?
XMFLOAT4X4 Camera::GetViewMatrix() const
{
	const XMVECTOR positionVec = XMLoadFloat3(&m_position);
	const XMVECTOR upVec = XMLoadFloat3(&m_up);
	const XMVECTOR forwardVec = XMLoadFloat3(&m_forward);

	XMFLOAT4X4 viewMatrix = {};
	XMStoreFloat4x4(&viewMatrix, XMMatrixLookAtRH(positionVec, positionVec + forwardVec, upVec));
	return viewMatrix;
}


XMFLOAT4X4 Camera::GetProjectionMatrix(const float appAspect) const
{
	XMFLOAT4X4 projectionMatrix = {};
	XMStoreFloat4x4(&projectionMatrix, XMMatrixPerspectiveFovRH(kFovY, appAspect, kNearClipPlane, kFarClipPlane));
	return projectionMatrix;
}


XMFLOAT3 Camera::GetPosition() const
{
	return m_position;
}


void Camera::UpdateOrientation()
{
	const XMVECTORF32 forwardVec = {
		XMScalarCos(m_yaw) * XMScalarCos(m_pitch),
		XMScalarSin(m_pitch),
		XMScalarSin(m_yaw) * XMScalarCos(m_pitch),
		1.0f
	};
	XMStoreFloat3(&m_forward, forwardVec);

	XMStoreFloat3(&m_right, XMVector3Normalize(XMVector3Cross(forwardVec, kDefaultUp)));
	const XMVECTOR rightVec = XMLoadFloat3(&m_right);
	XMStoreFloat3(&m_up, XMVector3Normalize(XMVector3Cross(rightVec, forwardVec)));
}
