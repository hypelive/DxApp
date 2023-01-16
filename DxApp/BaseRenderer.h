#pragma once

#include <stdint.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>

#include "Scene.h"


using namespace Microsoft::WRL;

// TODO port Scene class from https://github.com/hypelive/VulkanApp
// AND change signature to RenderScene(Viewport, Scene) ?
class BaseRenderer
{
public:
	BaseRenderer() = delete;
	explicit BaseRenderer(HWND hwnd, uint32_t windowWidth = 0, uint32_t windowHeight = 0);
	~BaseRenderer();

	void RenderScene(D3D12_VIEWPORT viewport);

	void SetScene(Scene* scene);

private:
	static constexpr uint32_t kSwapChainBuffersCount = 2;

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12Resource> m_renderTargets[kSwapChainBuffersCount];
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12Resource> m_depthStencilResources[kSwapChainBuffersCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[kSwapChainBuffersCount];
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	uint32_t m_rtvDescriptorSize = 0;
	uint32_t m_dsvDescriptorSize = 0;
	uint32_t m_cbvSrvUavDescriptorSize = 0;

	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
	ComPtr<ID3D12Resource> m_constantBufferUploadHeaps[kSwapChainBuffersCount];

	uint32_t m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	uint64_t m_fenceValues[kSwapChainBuffersCount] = { 0 };

	Scene* m_scene;

	uint32_t m_windowWidth;
	uint32_t m_windowHeight;

	uint8_t* m_cbDataCPU = nullptr;

	void LoadPipeline(HWND hwnd);
	void LoadAssets();
	void CreateRootDescriptorTableResources();

	// cb data, sceneObjectsData, lightsData, etc
	void UpdateData(float appAspect);

	void PopulateCommandList(D3D12_VIEWPORT viewport) const;

	void WaitForGpu();
	void UpdateToNextFrame();
};
