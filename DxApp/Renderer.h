#pragma once

#include <stdint.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_4.h>
#include <wrl.h>

#include "Scene.h"
#include "GBuffer.h"
#include "GeometryPass.h"
#include "LightingPass.h"


using namespace Microsoft::WRL;


struct CD3DX12_RESOURCE_BARRIER;


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

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_swapChainRtvHeap;
	ComPtr<ID3D12Resource> m_swapChainRenderTargets[kSwapChainBuffersCount];
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12Resource> m_depthStencilResources[kSwapChainBuffersCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocators[kSwapChainBuffersCount];

	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	uint32_t m_rtvDescriptorSize = 0;
	uint32_t m_dsvDescriptorSize = 0;
	uint32_t m_cbvSrvUavDescriptorSize = 0;

	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
	ComPtr<ID3D12Resource> m_constantBufferUploadHeaps[kSwapChainBuffersCount];

	uint32_t m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	uint64_t m_fenceValues[kSwapChainBuffersCount] = {0};

	Scene* m_scene;

	uint32_t m_windowWidth;
	uint32_t m_windowHeight;

	GBuffer m_gBuffer;
	GeometryPass m_geometryPass;
	LightingPass m_lightingPass;

	void LoadPipeline(HWND hwnd);
	void EnableDebugLayer();
	void CreateDevice(IDXGIFactory4* factory);
	void CreateCommandQueue();
	void CreateSwapChain(IDXGIFactory4* factory, HWND hwnd);
	void CreateDescriptorHeaps();
	// Creates Rts, Dss, CommandAllocators
	void CreateFrameResources();

	void LoadAssets();
	void CreateCommandList();
	void CreateSynchronizationResources();

	void CopyFrameResourcesToGpu();

	void CreateRootDescriptorTableResources();

	// cb data, sceneObjectsData, lightsData, etc
	void UpdateData(float appAspect);

	void PopulateCommandList(D3D12_VIEWPORT viewport);
	void AddGeometryPass(ID3D12GraphicsCommandList* commandList);
	void AddLightingPass(ID3D12GraphicsCommandList* commandList);

	void WaitForGpu();
	void UpdateToNextFrame();
};
