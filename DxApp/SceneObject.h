#pragma once

#include <wrl.h>
#include <chrono>
#include <cstdint>
#include <DirectXMath.h>
#include <d3d12.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>


using namespace Microsoft::WRL;

// Static scene object
class SceneObject
{
public:
	SceneObject() = delete;
	explicit SceneObject(aiMesh* mesh, aiMatrix4x4 transformMatrix);

	void CreateRenderResources(ID3D12Device* device);
	void DestroyRendererResources();

	uint32_t GetIndicesCount() const { return static_cast<uint32_t>(m_indices.size()); }

	D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() { return m_vertexBufferView; }
	D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() { return m_indexBufferView; }

	DirectX::XMFLOAT4X4& GetTransformMatrix() { return m_transformMatrix; }

private:
	struct Vertex
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};

	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;

	// DirectX resources:
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	DirectX::XMFLOAT4X4 m_transformMatrix;
};
