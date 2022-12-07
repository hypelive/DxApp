#pragma once

#include "stdint.h"
#include "d3d12.h"
#include "dxgi.h"


class BaseRenderer
{
public:
	BaseRenderer() = delete;
	explicit BaseRenderer(HWND hwnd);
	~BaseRenderer();

private:
	static constexpr uint32_t kSwapChainBuffersCount = 2;

	ID3D12Device* m_device = nullptr;
	ID3D12CommandQueue* m_commandQueue = nullptr;
	IDXGISwapChain* m_swapChain = nullptr;
	ID3D12DescriptorHeap* m_rtvHeap = nullptr;
	ID3D12Resource* m_renderTargets[kSwapChainBuffersCount] = { nullptr };
	ID3D12CommandAllocator* m_commandAllocator = nullptr;

	uint32_t m_rtvDescriptorSize = 0;
	
};

