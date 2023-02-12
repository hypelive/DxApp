#pragma once

#include <vector>

#include "SceneObject.h"
#include "Camera.h"
#include "LightSources.h"


class Scene
{
public:
	Scene() = delete;
	explicit Scene(const char* path);

	void CreateRendererResources(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	void DestroyUploadResources();
	void DestroyRendererResources();

	Camera& GetCamera() { return m_camera; }
	std::vector<SceneObject>& GetSceneObjects() { return m_sceneObjects; }
	uint32_t GetSceneObjectsCount() const { return static_cast<uint32_t>(m_sceneObjects.size()); }

private:
	std::vector<SceneObject> m_sceneObjects;
	Camera m_camera;
	LightSources m_lightSources;
};
