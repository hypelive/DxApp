#include "LightSources.h"


void LightSources::SetAmbient(AmbientLightSource lightSource)
{
	m_ambient = lightSource;
}


void LightSources::AddDirectional(DirectionalLightSource lightSource)
{
	assert(m_directionalLightSourcesCount < kMaxDirectionalLightSourcesCount);
	m_directionalSources[m_directionalLightSourcesCount++] = lightSource;
}


void LightSources::AddPoint(PointLightSource lightSource)
{
	assert(m_pointLightSourcesCount < kMaxPointLightSourcesCount);
	m_pointLightSources[m_pointLightSourcesCount++] = lightSource;
}
