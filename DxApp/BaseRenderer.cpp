#include <vector>

#include <d3d12sdklayers.h>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include "DxHelpers.h"
#include "SceneObjectConstantBuffer.h"
#include "BaseRenderer.h"


using namespace DxHelper;
using namespace DirectX;
using namespace Microsoft::WRL;


void GetHardwareAdapter(IDXGIFactory* factory, IDXGIAdapter** adapter)
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


BaseRenderer::BaseRenderer(HWND hwnd, uint32_t windowWidth, uint32_t windowHeight) : m_windowWidth(windowWidth),
	m_windowHeight(windowHeight)
{
	LoadPipeline(hwnd);
	LoadAssets();
}


BaseRenderer::~BaseRenderer()
{
	WaitForGpu();
	CloseHandle(m_fenceEvent);

	if (m_cbDataCPU)
		free(m_cbDataCPU);
	m_scene->DestroyRendererResources();
}


void BaseRenderer::RenderScene(D3D12_VIEWPORT viewport)
{
	UpdateData(viewport.Width / viewport.Height);

	PopulateCommandList(viewport);

	ID3D12CommandList* commandLists[] = {m_commandList.Get()};
	m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	DxVerify(m_swapChain->Present(0, 0));

	UpdateToNextFrame();
}


void BaseRenderer::SetScene(Scene* scene)
{
	m_scene = scene;

	m_scene->CreateRendererResources(m_device.Get());

	CreateRootDescriptorTableResources();
}


void BaseRenderer::LoadPipeline(HWND hwnd)
{
	// Debug layers
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}

	ComPtr<IDXGIFactory4> factory;
	DxVerify(CreateDXGIFactory(IID_PPV_ARGS(&factory)));

	// Device
	{
		ComPtr<IDXGIAdapter> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		DxVerify(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
	}

	// Command queue
	{
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		DxVerify(m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue)));
	}

	// Swap chain
	{
		ComPtr<IDXGISwapChain> swapChain;
		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferCount = kSwapChainBuffersCount;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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

	// Descriptor heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = kSwapChainBuffersCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		DxVerify(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = kSwapChainBuffersCount;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		DxVerify(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

		m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}

	// Frame resources
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (uint32_t n = 0; n < kSwapChainBuffersCount; n++)
		{
			DxVerify(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}

		CD3DX12_HEAP_PROPERTIES dsHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
		auto dsResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_windowWidth, m_windowHeight);
		dsResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		D3D12_CLEAR_VALUE dsClearValue = {};
		dsClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		dsClearValue.DepthStencil.Depth = 1.0;
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
	}

	for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
	{
		DxVerify(
			m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
	}
}


void BaseRenderer::LoadAssets()
{
	// Root signature
	{
		D3D12_DESCRIPTOR_RANGE descriptorRanges[1] = {};
		descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		descriptorRanges[0].NumDescriptors = 1;
		descriptorRanges[0].BaseShaderRegister = 0;
		descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		descriptorRanges[0].RegisterSpace = 0; // ?

		D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable = {};
		descriptorTable.NumDescriptorRanges = 1;
		descriptorTable.pDescriptorRanges = descriptorRanges;

		D3D12_ROOT_PARAMETER rootParameters[1] = {};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[0].DescriptorTable = descriptorTable;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr,
		                       D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;

		DxVerify(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature,
		                                     &error));
		DxVerify(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
		                                       IID_PPV_ARGS(&m_rootSignature)));
	}

	// Pipeline state object
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		uint32_t compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		uint32_t compileFlags = 0;
#endif

		DxVerify(D3DCompileFromFile(L"Shaders/Example_vs.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vs_main",
		                            "vs_5_0",
		                            compileFlags, 0, &vertexShader, nullptr));
		DxVerify(D3DCompileFromFile(L"Shaders/Example_ps.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main",
		                            "ps_5_0",
		                            compileFlags, 0, &pixelShader, nullptr));

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{
				"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
			}
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = {inputElementDescs, _countof(inputElementDescs)};
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = {static_cast<uint8_t*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize()};
		psoDesc.PS = {static_cast<uint8_t*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize()};
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState.FrontCounterClockwise = true;
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = true;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.StencilEnable = false;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;

		DxVerify(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	// Command list
	// TODO Device4::CreateCommandList1 - creates closed list
	{
		DxVerify(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(),
		                                     m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
		DxVerify(m_commandList->Close());
	}

	// Synchronization
	{
		DxVerify(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

		m_fenceValues[m_frameIndex] = 1;

		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			DxVerify(HRESULT_FROM_WIN32(GetLastError()));
		}

		WaitForGpu();
	}
}


void BaseRenderer::CreateRootDescriptorTableResources()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = kSwapChainBuffersCount * m_scene->GetSceneObjectsCount();
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	DxVerify(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));

	auto descriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
	{
		const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(1024 * 64); // alignment
		DxVerify(m_device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_constantBufferUploadHeaps[i])));
		m_constantBufferUploadHeaps[i]->SetName(L"Constant Buffer Upload Resource Heap");

		auto cbAddress = m_constantBufferUploadHeaps[i]->GetGPUVirtualAddress();
		for (uint32_t j = 0; j < m_scene->GetSceneObjectsCount(); j++)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = SceneObjectConstantBuffer::GetAlignedSize();
			m_device->CreateConstantBufferView(&cbvDesc, descriptorHandle);

			descriptorHandle.Offset(1, m_cbvSrvUavDescriptorSize);
			cbAddress += SceneObjectConstantBuffer::GetAlignedSize();
		}
	}
}


void BaseRenderer::UpdateData(float appAspect)
{
	if (!m_cbDataCPU)
		m_cbDataCPU = static_cast<uint8_t*>(malloc(
			SceneObjectConstantBuffer::GetAlignedSize() * m_scene->GetSceneObjectsCount()));

	auto currentCbPointer = m_cbDataCPU;
	for (uint32_t i = 0; i < m_scene->GetSceneObjects().size(); i++)
	{
		new(currentCbPointer) SceneObjectConstantBuffer();
		auto& sceneObjectData = *reinterpret_cast<SceneObjectConstantBuffer*>(currentCbPointer);

		sceneObjectData.model = m_scene->GetSceneObjects()[i].GetTransformMatrix();
		sceneObjectData.view = m_scene->GetCamera().GetViewMatrix();
		sceneObjectData.projection = m_scene->GetCamera().GetProjectionMatrix(appAspect);

		XMMATRIX modelMatrix = XMLoadFloat4x4(&sceneObjectData.model);
		XMMATRIX viewMatrix = XMLoadFloat4x4(&sceneObjectData.view);
		XMMATRIX projectionMatrix = XMLoadFloat4x4(&sceneObjectData.projection);

		XMMATRIX vpMatrix = XMMatrixMultiply(viewMatrix, projectionMatrix);

		XMStoreFloat4x4(&sceneObjectData.vp, vpMatrix);
		XMStoreFloat4x4(&sceneObjectData.mvp, XMMatrixMultiply(modelMatrix, vpMatrix));

		currentCbPointer += SceneObjectConstantBuffer::GetAlignedSize();
	}

	uint8_t* cbDataGPU;
	const auto readRange = CD3DX12_RANGE(0, 0);
	DxVerify(m_constantBufferUploadHeaps[m_frameIndex]->Map(0, &readRange, reinterpret_cast<void**>(&cbDataGPU)));
	memcpy(cbDataGPU, m_cbDataCPU, SceneObjectConstantBuffer::GetAlignedSize() * m_scene->GetSceneObjectsCount());
	m_constantBufferUploadHeaps[m_frameIndex]->Unmap(0, nullptr);
}


void BaseRenderer::PopulateCommandList(D3D12_VIEWPORT viewport) const
{
	auto* commandList = m_commandList.Get();

	DxVerify(m_commandAllocators[m_frameIndex]->Reset());
	DxVerify(commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get()));

	commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	D3D12_VIEWPORT viewports[] = {viewport};
	commandList->RSSetViewports(1, viewports);
	D3D12_RECT scissorsRects[] = {
		{0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height)}
	};
	commandList->RSSetScissorRects(1, scissorsRects);

	ID3D12DescriptorHeap* descriptorHeaps[] = {m_descriptorHeap.Get()};
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	auto cbDescriptorTable = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
	cbDescriptorTable.Offset(m_frameIndex * m_scene->GetSceneObjectsCount(), m_cbvSrvUavDescriptorSize);

	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
	                                                    D3D12_RESOURCE_STATE_PRESENT,
	                                                    D3D12_RESOURCE_STATE_RENDER_TARGET);
	commandList->ResourceBarrier(1, &barrier);

	auto rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex,
	                                               m_rtvDescriptorSize);
	auto dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex,
	                                               m_dsvDescriptorSize);
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	const float kClearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
	commandList->ClearRenderTargetView(rtvHandle, kClearColor, 0, nullptr);
	const float kClearDepth = 1.0f;
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, kClearDepth, 0, 0, nullptr);

	for (auto& sceneObject : m_scene->GetSceneObjects())
	{
		commandList->SetGraphicsRootDescriptorTable(0, cbDescriptorTable);
		cbDescriptorTable.Offset(1, m_cbvSrvUavDescriptorSize);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &sceneObject.GetVertexBufferView());
		commandList->IASetIndexBuffer(&sceneObject.GetIndexBufferView());

		commandList->DrawIndexedInstanced(sceneObject.GetIndicesCount(), 1, 0, 0, 0);
	}

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
	                                               D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	commandList->ResourceBarrier(1, &barrier);

	DxVerify(commandList->Close());
}


void BaseRenderer::WaitForGpu()
{
	DxVerify(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

	DxVerify(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
	WaitForSingleObject(m_fenceEvent, INFINITE);

	m_fenceValues[m_frameIndex]++;
}


void BaseRenderer::UpdateToNextFrame()
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
