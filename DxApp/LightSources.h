#pragma once

#include <DirectXMath.h>


struct LightSource
{
	// w unused
	DirectX::XMFLOAT4 color;

	LightSource() : color(0.0f, 0.0f, 0.0f, 1.0f)
	{
	};

	LightSource(DirectX::XMFLOAT4 lightColor) :
		color(lightColor)
	{
	};
};


struct AmbientLightSource : LightSource
{
	AmbientLightSource() = default;

	AmbientLightSource(DirectX::XMFLOAT4 lightColor) :
		LightSource(lightColor)
	{
	}
};


struct DirectionalLightSource : LightSource
{
	// w unused
	DirectX::XMFLOAT4 direction;

	DirectionalLightSource() : direction(0.0f, 0.0f, 0.0f, 1.0f)
	{
	};

	DirectionalLightSource(DirectX::XMFLOAT4 lightColor, DirectX::XMFLOAT4 lightDirection) :
		LightSource(lightColor), direction(lightDirection)
	{
	}
};


struct PointLightSource : LightSource
{
	// w unused
	DirectX::XMFLOAT4 position;

	PointLightSource() : position(0.0f, 0.0f, 0.0f, 1.0f)
	{
	};

	PointLightSource(DirectX::XMFLOAT4 lightColor, DirectX::XMFLOAT4 lightPosition) :
		LightSource(lightColor), position(lightPosition)
	{
	}
};


class LightSources
{
public:
	static constexpr uint32_t kMaxDirectionalLightSourcesCount = 4;
	static constexpr uint32_t kMaxPointLightSourcesCount = 8;

	LightSources() = default;

	void SetAmbient(AmbientLightSource lightSource);
	void AddDirectional(DirectionalLightSource lightSource);
	void AddPoint(PointLightSource lightSource);

private:
	AmbientLightSource m_ambient;
	DirectionalLightSource m_directionalSources[kMaxDirectionalLightSourcesCount];
	PointLightSource m_pointLightSources[kMaxPointLightSourcesCount];

	uint32_t m_directionalLightSourcesCount = 0;
	uint32_t m_pointLightSourcesCount = 0;
};
