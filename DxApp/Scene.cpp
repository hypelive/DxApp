#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Scene.h"


Scene::Scene(const char* path) : m_camera(XMFLOAT3(-1.0f, -1.5f, 0.5f))
{
	auto* importedScene = aiImportFile(path, aiProcessPreset_TargetRealtime_MaxQuality);
	assert(importedScene);

	if (importedScene->HasMeshes())
	{
		struct StackEntry
		{
			aiNode* node;
			aiMatrix4x4 parentTransform;
		};

		std::vector<StackEntry> stack;
		stack.push_back({importedScene->mRootNode, aiMatrix4x4()});

		while (!stack.empty())
		{
			auto entry = stack.back();
			stack.pop_back();

			auto transform = entry.node->mTransformation * entry.parentTransform;

			for (uint32_t i = 0; i < entry.node->mNumMeshes; i++)
			{
				auto* mesh = importedScene->mMeshes[entry.node->mMeshes[i]];

				m_sceneObjects.push_back(SceneObject(mesh, transform));
			}

			for (uint32_t i = 0; i < entry.node->mNumChildren; i++)
				stack.push_back({entry.node->mChildren[i], transform});
		}
	}

	if (importedScene->HasLights())
	{
		for (uint32_t i = 0; i < importedScene->mNumLights; i++)
		{
			auto* light = importedScene->mLights[i];

			switch (light->mType)
			{
			case aiLightSource_AMBIENT:
				m_lightSources.SetAmbient(AmbientLightSource(
					XMFLOAT4(light->mColorAmbient.r, light->mColorAmbient.g, light->mColorAmbient.b, 1.0f)));
				break;
			case aiLightSource_DIRECTIONAL:
				m_lightSources.AddDirectional(DirectionalLightSource(
					XMFLOAT4(light->mColorDiffuse.r, light->mColorDiffuse.g, light->mColorDiffuse.b, 1.0f),
					XMFLOAT4(light->mDirection.x, light->mDirection.y, light->mDirection.z, 1.0f)));
				break;
			case aiLightSource_POINT:
				m_lightSources.AddPoint(PointLightSource(
					XMFLOAT4(light->mColorDiffuse.r, light->mColorDiffuse.g, light->mColorDiffuse.b, 1.0f),
					XMFLOAT4(light->mPosition.x, light->mPosition.y, light->mPosition.z, 1.0f)));
				break;
			case aiLightSource_AREA:
				// TODO
				break;
			default:
				break;
			}
		}
	}
	else
	{
		// Create our own light sources
		m_lightSources.SetAmbient(AmbientLightSource(
			XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f)));
		m_lightSources.AddDirectional(DirectionalLightSource(
			XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
			XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)));
		m_lightSources.AddPoint(PointLightSource(
			XMFLOAT4(10.0f, 10.0f, 10.0f, 1.0f),
			XMFLOAT4(2.0f, 0.0f, 0.0f, 1.0f)));
	}
}


void Scene::CreateRendererResources(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	for (auto& sceneObject : m_sceneObjects)
		sceneObject.CreateRenderResources(device, commandList);
}

void Scene::DestroyUploadResources()
{
	for (auto& sceneObject : m_sceneObjects)
		sceneObject.DestroyUploadResources();
}


void Scene::DestroyRendererResources()
{
	for (auto& sceneObject : m_sceneObjects)
		sceneObject.DestroyRendererResources();
}
