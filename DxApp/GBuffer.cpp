#include "GBuffer.h"
#include "DxHelpers.h"


void GBuffer::CreateResources(ID3D12Device* device, uint32_t windowWidth, uint32_t windowHeight)
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = kRtCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DxVerify(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

	auto rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	const uint32_t rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, windowWidth, windowHeight,
		1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	D3D12_CLEAR_VALUE clearValue = { DXGI_FORMAT_R8G8B8A8_UNORM, {0.0f, 0.0f, 0.0f, 1.0f} };

	const CD3DX12_HEAP_PROPERTIES gBufferHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

	DxVerify(device->CreateCommittedResource(&gBufferHeapProperties,
		D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, &resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue,
		IID_PPV_ARGS(&m_surfaceColorRt)));
	m_surfaceColorRt->SetName(TEXT("GBuffer::SurfaceColorRt"));
	device->CreateRenderTargetView(m_surfaceColorRt.Get(), nullptr, rtvHandle);
	rtvHandle.Offset(1, rtvDescriptorSize);

	resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R16G16B16A16_FLOAT, windowWidth, windowHeight,
		1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	clearValue.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

	DxVerify(device->CreateCommittedResource(&gBufferHeapProperties,
		D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, &resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue,
		IID_PPV_ARGS(&m_positionRoughnessRt)));
	m_positionRoughnessRt->SetName(TEXT("GBuffer::PositionRoughnessRt"));
	device->CreateRenderTargetView(m_positionRoughnessRt.Get(), nullptr, rtvHandle);
	rtvHandle.Offset(1, rtvDescriptorSize);

	DxVerify(device->CreateCommittedResource(&gBufferHeapProperties,
		D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, &resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue,
		IID_PPV_ARGS(&m_normalMetalnessRt)));
	m_normalMetalnessRt->SetName(TEXT("GBuffer::NormalMetalnessRt"));
	device->CreateRenderTargetView(m_normalMetalnessRt.Get(), nullptr, rtvHandle);
	rtvHandle.Offset(1, rtvDescriptorSize);

	resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, windowWidth, windowHeight,
		1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	DxVerify(device->CreateCommittedResource(&gBufferHeapProperties,
		D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, &resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, &clearValue,
		IID_PPV_ARGS(&m_fresnelIndicesRt)));
	m_fresnelIndicesRt->SetName(TEXT("GBuffer::FresnelIndicesRt"));
	device->CreateRenderTargetView(m_fresnelIndicesRt.Get(), nullptr, rtvHandle);
	rtvHandle.Offset(1, rtvDescriptorSize);
}


void GBuffer::AddBarriers(CD3DX12_RESOURCE_BARRIER* array, D3D12_RESOURCE_STATES stateBefore,
	D3D12_RESOURCE_STATES stateAfter) const
{
	array[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_surfaceColorRt.Get(),
		stateBefore,
		stateAfter);
	array[1] = CD3DX12_RESOURCE_BARRIER::Transition(m_positionRoughnessRt.Get(),
		stateBefore,
		stateAfter);
	array[2] = CD3DX12_RESOURCE_BARRIER::Transition(m_normalMetalnessRt.Get(),
		stateBefore,
		stateAfter);
	array[3] = CD3DX12_RESOURCE_BARRIER::Transition(m_fresnelIndicesRt.Get(),
		stateBefore,
		stateAfter);
}
