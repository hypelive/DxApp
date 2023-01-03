#pragma once

#include <DirectXMath.h>

using namespace DirectX;

struct SceneObjectConstantBuffer
{
	XMFLOAT4X4 model;
	XMFLOAT4X4 view;
	XMFLOAT4X4 projection;

	XMFLOAT4X4 mvp;

	XMFLOAT4X4 vp;
};