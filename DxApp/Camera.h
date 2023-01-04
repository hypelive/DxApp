#pragma once

#include <DirectXMath.h>


using namespace DirectX;

class Camera
{
public:
	Camera(XMFLOAT3 position);
	Camera() = delete;

	void Translate(XMFLOAT3 offset);
	void Rotate(XMFLOAT2 angles);

	XMFLOAT4X4 GetViewMatrix() const;
	XMFLOAT4X4 GetProjectionMatrix(float appAspect) const;
	XMFLOAT3 GetPosition() const;

private:
	XMFLOAT3 m_position{};

	float m_pitch;
	float m_yaw;

	XMFLOAT3 m_forward{};
	XMFLOAT3 m_up{};
	XMFLOAT3 m_right{};

	const XMVECTORF32 kDefaultUp = { 0.0f, 1.0f, 0.0f, 1.0f };
	const float kEpsilon = XM_PI / 180.0f;
	const float kFovY = XMConvertToRadians(60.0f);
	const float kNearClipPlane = 0.1f;
	const float kFarClipPlane = 1000.0f;

	void UpdateOrientation();
};

