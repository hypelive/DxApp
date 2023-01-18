#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Scene.h"


Scene::Scene(const char* path) : m_camera(XMFLOAT3(-1.0f, -1.5f, 0.5f))
{
	auto* importedScene = aiImportFile(path, aiProcessPreset_TargetRealtime_MaxQuality);
	assert(importedScene);

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
