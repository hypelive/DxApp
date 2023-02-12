#pragma once

#include <stdint.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>

#include "Scene.h"


using namespace Microsoft::WRL;

class Renderer
{
public:
	Renderer() = delete;
	explicit Renderer(HWND hwnd, uint32_t windowWidth = 0, uint32_t windowHeight = 0);
	~Renderer();

	void RenderScene(D3D12_VIEWPORT viewport);

	void SetScene(Scene* scene);

private:
	static constexpr uint32_t kSwapChainBuffersCount = 2;

	static constexpr DXGI_FORMAT kDsFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_swapChainRtvHeap;
	ComPtr<ID3D12Resource> m_swapChainRenderTargets[kSwapChainBuffersCount];
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

	struct GBuffer
	{
		static constexpr uint32_t kRtCount = 3;

		ComPtr<ID3D12DescriptorHeap> rtvHeap;
		ComPtr<ID3D12Resource> albedoRt;
		ComPtr<ID3D12Resource> positionRt;
		ComPtr<ID3D12Resource> normalRt;

		// TODO pack metal, rougness, emissive
	};

	GBuffer m_gBuffer;

	void LoadPipeline(HWND hwnd);
	void EnableDebugLayer();
	void CreateDevice(IDXGIFactory4* factory);
	void CreateCommandQueue();
	void CreateSwapChain(IDXGIFactory4* factory, HWND hwnd);
	void CreateDescriptorHeaps();
	// Creates Rts, Dss, CommandAllocators
	void CreateFrameResources();

	void LoadAssets();
	void CreateRootSignature();
	void CreatePipelineStateObject();
	void CreateCommandList();
	void CreateSynchronizationResources();

	void CreateRootDescriptorTableResources();

	// cb data, sceneObjectsData, lightsData, etc
	void UpdateData(float appAspect);

	void PopulateCommandList(D3D12_VIEWPORT viewport) const;

	void WaitForGpu();
	void UpdateToNextFrame();
};