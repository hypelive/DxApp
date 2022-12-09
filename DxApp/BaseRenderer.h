#pragma once

#include "stdint.h"
#include "d3d12.h"
#include "dxgi.h"
#include "wrl.h"


using namespace Microsoft::WRL;

class BaseRenderer
{
public:
	BaseRenderer() = delete;
	explicit BaseRenderer(HWND hwnd);
	~BaseRenderer();

private:
	static constexpr uint32_t kSwapChainBuffersCount = 2;

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<IDXGISwapChain> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12Resource> m_renderTargets[kSwapChainBuffersCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;

	uint32_t m_rtvDescriptorSize = 0;
	
};

