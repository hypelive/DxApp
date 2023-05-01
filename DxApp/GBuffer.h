#pragma once

#include <cstdint>
#include <d3d12.h>
#include <d3dx12.h>
#include <wrl.h>


class GBuffer
{
public:
	static constexpr DXGI_FORMAT kRtFormats[] = {
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		DXGI_FORMAT_R8G8B8A8_UNORM
	};
	static constexpr uint32_t kRtCount = static_cast<uint32_t>(std::size(kRtFormats));

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_surfaceColorRt;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_positionRoughnessRt;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_normalMetalnessRt;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_fresnelIndicesRt;

	void CreateResources(ID3D12Device* device, uint32_t windowWidth, uint32_t windowHeight);
	void AddBarriers(CD3DX12_RESOURCE_BARRIER* array, D3D12_RESOURCE_STATES stateBefore,
		D3D12_RESOURCE_STATES stateAfter) const;
};

