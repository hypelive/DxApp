#include <d3dx12.h>

#include "DxHelpers.h"
#include "SceneObject.h"


using namespace DirectX;
using namespace DxHelper;


SceneObject::SceneObject(aiMesh* mesh, aiMatrix4x4 transformMatrix)
{
	m_vertices.resize(mesh->mNumVertices);
	for (uint32_t n = 0; n < mesh->mNumVertices; n++)
	{
		const auto& position = mesh->mVertices[n];
		const auto& color = mesh->mColors[0][n];

		m_vertices[n].position = XMFLOAT3(position.x, position.y, position.z);
		m_vertices[n].color = XMFLOAT4(color.r, color.g, color.b, color.a);
	}

	// Support only triangles
	m_indices.reserve(mesh->mNumFaces * 3);
	for (uint32_t primitiveIndex = 0; primitiveIndex < mesh->mNumFaces; primitiveIndex++)
	{
		const auto& primitive = mesh->mFaces[primitiveIndex];

		for (uint32_t n = 0; n < primitive.mNumIndices; n++)
		{
			m_indices.push_back(primitive.mIndices[n]);
		}
	}

	m_transformMatrix = XMFLOAT4X4(reinterpret_cast<const float*>(&transformMatrix.Transpose()));
}


void SceneObject::CreateRenderResources(ID3D12Device* device)
{
	const uint32_t vertexBufferSize = static_cast<uint32_t>(m_vertices.size()) * sizeof(Vertex);
	const uint32_t indexBufferSize = static_cast<uint32_t>(m_indices.size()) * sizeof(uint32_t);

	// TODO DefaultHeap to not upload this buffers every frame?
	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
	const auto vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
	const auto indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
	DxVerify(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc,
	                                         D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
	                                         IID_PPV_ARGS(&m_vertexBuffer)));
	DxVerify(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &indexBufferDesc,
	                                         D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_indexBuffer)));

	uint8_t* dataPointer;
	const auto readRange = CD3DX12_RANGE(0, 0);

	DxVerify(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&dataPointer)));
	memcpy(dataPointer, m_vertices.data(), vertexBufferSize);
	m_vertexBuffer->Unmap(0, nullptr);

	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = vertexBufferSize;
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);

	DxVerify(m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&dataPointer)));
	memcpy(dataPointer, m_indices.data(), indexBufferSize);
	m_indexBuffer->Unmap(0, nullptr);

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.SizeInBytes = indexBufferSize;
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
}


void SceneObject::DestroyRendererResources()
{
	m_vertexBufferView = {};
	m_vertexBuffer.Reset();
	m_indexBufferView = {};
	m_indexBuffer.Reset();
}
