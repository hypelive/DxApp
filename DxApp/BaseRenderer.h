#pragma once

#include "stdint.h"
#include "d3d12.h"
#include "dxgi1_4.h"
#include "wrl.h"


using namespace Microsoft::WRL;

class BaseRenderer
{
public:
	BaseRenderer() = delete;
	explicit BaseRenderer(HWND hwnd);
	~BaseRenderer();

	void RenderScene(D3D12_VIEWPORT viewport);

private:
	static constexpr uint32_t kSwapChainBuffersCount = 2;

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12Resource> m_renderTargets[kSwapChainBuffersCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	uint32_t m_rtvDescriptorSize = 0;

	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	uint32_t m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	uint64_t m_fenceValue;

	void LoadPipeline(HWND hwnd);
	void LoadAssets();
	void LoadScene();

	void PopulateCommandList(D3D12_VIEWPORT viewport);

	void WaitForPreviousFrame();
};
