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


void SceneObject::CreateRenderResources(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	const uint32_t vertexBufferSize = static_cast<uint32_t>(m_vertices.size()) * sizeof(Vertex);
	const uint32_t indexBufferSize = static_cast<uint32_t>(m_indices.size()) * sizeof(uint32_t);

	const auto vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
	const auto indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);

	// Create and fill upload heaps
	{
		const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		DxVerify(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&m_vertexBufferUpload)));
		DxVerify(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &indexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_indexBufferUpload)));

		uint8_t* dataPointer;
		const auto readRange = CD3DX12_RANGE(0, 0);

		DxVerify(m_vertexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&dataPointer)));
		memcpy(dataPointer, m_vertices.data(), vertexBufferSize);
		m_vertexBufferUpload->Unmap(0, nullptr);

		DxVerify(m_indexBufferUpload->Map(0, &readRange, reinterpret_cast<void**>(&dataPointer)));
		memcpy(dataPointer, m_indices.data(), indexBufferSize);
		m_indexBufferUpload->Unmap(0, nullptr);
	}

	// Create Default heap buffers and write copy commands to commandList
	{
		const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		DxVerify(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &vertexBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)));
		DxVerify(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &indexBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
			IID_PPV_ARGS(&m_indexBuffer)));

		commandList->CopyResource(m_vertexBuffer.Get(), m_vertexBufferUpload.Get());
		commandList->CopyResource(m_indexBuffer.Get(), m_indexBufferUpload.Get());

		const CD3DX12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
												 D3D12_RESOURCE_STATE_GENERIC_READ),
			CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
												 D3D12_RESOURCE_STATE_GENERIC_READ)
		};
		commandList->ResourceBarrier(2, barriers);
	}

	// Create vertex and index buffer descriptors
	{
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.SizeInBytes = vertexBufferSize;
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);

		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.SizeInBytes = indexBufferSize;
		m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	}
}


void SceneObject::DestroyUploadResources()
{
	m_vertexBufferUpload.Reset();
	m_indexBufferUpload.Reset();
}


void SceneObject::DestroyRendererResources()
{
	m_vertexBufferView = {};
	m_vertexBufferUpload.Reset();
	m_indexBufferView = {};
	m_indexBufferUpload.Reset();
}
