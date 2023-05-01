#include "Renderer.h"

#include <vector>
#include <iterator>

#include <d3d12sdklayers.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include "DxHelpers.h"
#include "Ltcs.h"
#include "Renderer.h"

#include <array>

#include "GeometryPassObjectConstantBuffer.h"
#include "RendererForwards.h"


using namespace DirectX;
using namespace Microsoft::WRL;


//TODO

// add debug light draw
// add disk area light (as spotlight)
// add sphere lights
// add rect area lights


static void GetHardwareAdapter(IDXGIFactory* factory, IDXGIAdapter** adapter)
{
	*adapter = nullptr;
	for (UINT adapterIndex = 0; ; ++adapterIndex)
	{
		IDXGIAdapter* pAdapter = nullptr;
		if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters(adapterIndex, &pAdapter))
		{
			// No more adapters to enumerate.
			break;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr)))
		{
			*adapter = pAdapter;
			return;
		}
	}
}


Renderer::Renderer(HWND hwnd, uint32_t windowWidth, uint32_t windowHeight) : m_windowWidth(windowWidth),
                                                                             m_windowHeight(windowHeight)
{
	LoadPipeline(hwnd);
	LoadAssets();

	CopyFrameResourcesToGpu();
}


Renderer::~Renderer()
{
	WaitForGpu();
	CloseHandle(m_fenceEvent);
	m_scene->DestroyRendererResources();
}


void Renderer::RenderScene(D3D12_VIEWPORT viewport)
{
	UpdateData(viewport.Width / viewport.Height);

	PopulateCommandList(viewport);

	ID3D12CommandList* commandLists[] = {m_commandList.Get()};
	m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	DxVerify(m_swapChain->Present(0, 0));

	UpdateToNextFrame();
}


void Renderer::SetScene(Scene* scene)
{
	m_scene = scene;

	// Loading mesh data to the GPU
	DxVerify(m_commandAllocators[m_frameIndex]->Reset());
	DxVerify(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

	m_scene->CreateRendererResources(m_device.Get(), m_commandList.Get());

	DxVerify(m_commandList->Close());

	ID3D12CommandList* commandLists[] = {m_commandList.Get()};
	m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
	WaitForGpu();

	// Free upload buffers memory
	m_scene->DestroyUploadResources();

	m_geometryPass.SetScene(m_scene);
	m_lightingPass.SetScene(m_scene);

	CreateRootDescriptorTableResources();
}


void Renderer::LoadPipeline(HWND hwnd)
{
#ifdef _DEBUG
	EnableDebugLayer();
#endif

	ComPtr<IDXGIFactory4> factory;
	DxVerify(CreateDXGIFactory(IID_PPV_ARGS(&factory)));

	CreateDevice(factory.Get());
	CreateCommandQueue();
	CreateSwapChain(factory.Get(), hwnd);
	CreateDescriptorHeaps();
	CreateFrameResources();
}


void Renderer::EnableDebugLayer()
{
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
}


void Renderer::CreateDevice(IDXGIFactory4* factory)
{
	ComPtr<IDXGIAdapter> hardwareAdapter;
	GetHardwareAdapter(factory, &hardwareAdapter);

	DxVerify(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
}


void Renderer::CreateCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	DxVerify(m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue)));
}


void Renderer::CreateSwapChain(IDXGIFactory4* factory, HWND hwnd)
{
	ComPtr<IDXGISwapChain> swapChain;
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = kSwapChainBuffersCount;
	swapChainDesc.BufferDesc.Format = kSwapChainFormat;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = TRUE;

	DxVerify(factory->CreateSwapChain(m_commandQueue.Get(), &swapChainDesc, &swapChain));

	DxVerify(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Turn off transition to full screen
	DxVerify(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
}


void Renderer::CreateDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = kSwapChainBuffersCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DxVerify(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_swapChainRtvHeap)));

	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = kSwapChainBuffersCount;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DxVerify(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

	m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}


void Renderer::CreateFrameResources()
{
	// SwapChain descriptors
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_swapChainRtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (uint32_t n = 0; n < kSwapChainBuffersCount; n++)
		{
			DxVerify(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_swapChainRenderTargets[n])));
			m_device->CreateRenderTargetView(m_swapChainRenderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}
	}

	// GBuffer 
	m_gBuffer.CreateResources(m_device.Get(), m_windowWidth, m_windowHeight);

	// Depth Stencil
	{
		const CD3DX12_HEAP_PROPERTIES dsHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		auto dsResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(kDsFormat, m_windowWidth, m_windowHeight);
		dsResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_CLEAR_VALUE dsClearValue = {};
		dsClearValue.Format = kDsFormat;
		dsClearValue.DepthStencil.Depth = 1.0f;
		dsClearValue.DepthStencil.Stencil = 0;

		CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
		for (uint32_t n = 0; n < kSwapChainBuffersCount; n++)
		{
			DxVerify(m_device->CreateCommittedResource(&dsHeapProperties, D3D12_HEAP_FLAG_NONE,
				&dsResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&dsClearValue, IID_PPV_ARGS(&m_depthStencilResources[n])));

			m_device->CreateDepthStencilView(m_depthStencilResources[n].Get(), nullptr, dsvHandle);
			dsvHandle.Offset(1, m_dsvDescriptorSize);
		}

		for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
		{
			DxVerify(
				m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
					IID_PPV_ARGS(&m_commandAllocators[i])));
		}
	}
}


void Renderer::LoadAssets()
{
	m_geometryPass.Initialize(m_device.Get());
	m_lightingPass.Initialize(m_device.Get());

	CreateCommandList();
	CreateSynchronizationResources();

	WaitForGpu();
}


void Renderer::CreateCommandList()
{
	DxVerify(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(),
		nullptr, IID_PPV_ARGS(&m_commandList)));
	DxVerify(m_commandList->Close());
}


void Renderer::CreateSynchronizationResources()
{
	DxVerify(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

	m_fenceValues[m_frameIndex] = 1;

	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		DxVerify(HRESULT_FROM_WIN32(GetLastError()));
	}
}


void Renderer::CopyFrameResourcesToGpu()
{
}


void Renderer::CreateRootDescriptorTableResources()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = kSwapChainBuffersCount * m_geometryPass.GetDescriptorTablesDescriptorsCount() + kSwapChainBuffersCount *
		m_lightingPass.GetDescriptorTablesDescriptorsCount();
	// plus one for LightSources CBV
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	DxVerify(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));

	auto descriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
	{
		// Create upload heaps
		{
			const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64); // WARNING, hardcoded size
			DxVerify(m_device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_constantBufferUploadHeaps[i])));
			m_constantBufferUploadHeaps[i]->SetName(L"Constant Buffer Upload Resource Heap");
		}

		const auto cbAddress = m_constantBufferUploadHeaps[i]->GetGPUVirtualAddress();
		m_geometryPass.SetupRootResourceDescriptors(m_device.Get(), descriptorHandle, cbAddress);
		descriptorHandle.Offset(m_geometryPass.GetDescriptorTablesDescriptorsCount(), m_cbvSrvUavDescriptorSize);
	}

	for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
	{
		const auto cbAddress = m_constantBufferUploadHeaps[i]->GetGPUVirtualAddress() + m_geometryPass.GetRootResourcesSize();
		m_lightingPass.SetupRootResourceDescriptors(m_device.Get(), descriptorHandle, cbAddress, m_gBuffer);
		descriptorHandle.Offset(m_lightingPass.GetDescriptorTablesDescriptorsCount(), m_cbvSrvUavDescriptorSize);
	}
}


void Renderer::UpdateData(float appAspect)
{
	uint8_t* cbDataGpu;
	const auto readRange = CD3DX12_RANGE(0, 0);
	DxVerify(m_constantBufferUploadHeaps[m_frameIndex]->Map(0, &readRange, reinterpret_cast<void**>(&cbDataGpu)));

	m_geometryPass.UpdateRootResources(cbDataGpu, appAspect);
	cbDataGpu += m_geometryPass.GetRootResourcesSize();

	m_lightingPass.UpdateRootResources(cbDataGpu);
	cbDataGpu += m_lightingPass.GetRootResourcesSize();

	m_constantBufferUploadHeaps[m_frameIndex]->Unmap(0, nullptr);
}


void Renderer::PopulateCommandList(D3D12_VIEWPORT viewport)
{
	auto* commandList = m_commandList.Get();

	DxVerify(m_commandAllocators[m_frameIndex]->Reset());
	DxVerify(commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr));

	DxHelper::SetRenderTarget(commandList, viewport);
	AddGeometryPass(commandList);
	AddLightingPass(commandList);

	const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_swapChainRenderTargets[m_frameIndex].Get(),
	                                                          D3D12_RESOURCE_STATE_RENDER_TARGET,
	                                                          D3D12_RESOURCE_STATE_PRESENT);
	commandList->ResourceBarrier(1, &barrier);

	DxVerify(commandList->Close());
}


void Renderer::AddGeometryPass(ID3D12GraphicsCommandList* commandList)
{
	m_geometryPass.Setup(commandList);

	ID3D12DescriptorHeap* descriptorHeaps[] = {m_descriptorHeap.Get()};
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	DxHelper::ResourceBarriersArray<GBuffer::kRtCount> barriers;
	m_gBuffer.AddBarriers(barriers.data(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());

	auto rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_gBuffer.m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandles[m_gBuffer.kRtCount];
	for (uint32_t i = 0; i < m_gBuffer.kRtCount; i++)
	{
		rtvHandles[i] = rtvHandle;
		rtvHandle.Offset(1, m_rtvDescriptorSize);
	}

	const auto dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(),
	                                                     m_frameIndex,
	                                                     m_dsvDescriptorSize);
	commandList->OMSetRenderTargets(m_gBuffer.kRtCount, rtvHandles, FALSE, &dsvHandle);

	for (uint32_t i = 0; i < m_gBuffer.kRtCount; i++)
	{
		constexpr float kClearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
		commandList->ClearRenderTargetView(rtvHandles[i], kClearColor, 0, nullptr);
	}
	constexpr float kClearDepth = 1.0f;
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, kClearDepth, 0,
	                                   0, nullptr);

	auto cbDescriptorTable = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
	cbDescriptorTable.Offset(m_frameIndex * m_geometryPass.GetDescriptorTablesDescriptorsCount(), m_cbvSrvUavDescriptorSize);

	m_geometryPass.Draw(commandList, cbDescriptorTable, m_scene);
}


void Renderer::AddLightingPass(ID3D12GraphicsCommandList* commandList)
{
	m_lightingPass.Setup(commandList);

	ID3D12DescriptorHeap* descriptorHeaps[] = {m_descriptorHeap.Get()};
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	DxHelper::ResourceBarriersArray<GBuffer::kRtCount + 1> barriers;
	m_gBuffer.AddBarriers(barriers.data(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
	barriers[GBuffer::kRtCount] = CD3DX12_RESOURCE_BARRIER::Transition(m_swapChainRenderTargets[m_frameIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());

	const auto rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_swapChainRtvHeap->GetCPUDescriptorHandleForHeapStart(),
	                                                     m_frameIndex, m_rtvDescriptorSize);
	const auto dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(),
	                                                     m_frameIndex,
	                                                     m_dsvDescriptorSize);
	commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
	constexpr float kClearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
	commandList->ClearRenderTargetView(rtvHandle, kClearColor, 0, nullptr);

	auto descriptorTable = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
	descriptorTable.Offset(
		kSwapChainBuffersCount * m_geometryPass.GetDescriptorTablesDescriptorsCount() + m_frameIndex * m_lightingPass.GetDescriptorTablesDescriptorsCount(),
		m_cbvSrvUavDescriptorSize);

	m_lightingPass.Draw(commandList, descriptorTable);
}


void Renderer::WaitForGpu()
{
	DxVerify(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	DxVerify(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
	WaitForSingleObject(m_fenceEvent, INFINITE);

	m_fenceValues[m_frameIndex]++;
}


void Renderer::UpdateToNextFrame()
{
	const uint64_t currentFenceValue = m_fenceValues[m_frameIndex];
	DxVerify(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		DxVerify(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}
