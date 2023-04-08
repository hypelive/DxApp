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


using namespace DirectX;
using namespace Microsoft::WRL;


//TODO

// add debug light draw
// add disk area light (as spotlight)
// add sphere lights
// add rect area lights

namespace
{
	struct LightingPassConstantBuffer
	{
		XMFLOAT4 cameraPosition;
		LightSources lightSources;

		static uint32_t GetAlignedSize() { return (sizeof(LightingPassConstantBuffer) + 255) & ~255; }
	};
}


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

	if (m_cbDataCPU)
		free(m_cbDataCPU);
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

	CreateRootDescriptorTableResources();
}


void Renderer::GBuffer::CreateResources(ID3D12Device* device, uint32_t windowWidth, uint32_t windowHeight)
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


void Renderer::GBuffer::AddBarriers(CD3DX12_RESOURCE_BARRIER* array, D3D12_RESOURCE_STATES stateBefore,
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
	CreateRootSignatures();
	CreatePipelineStateObjects();
	CreateCommandList();
	CreateSynchronizationResources();

	WaitForGpu();
}


void Renderer::CreateRootSignatures()
{
	CreateGeometryPassRootSignature();
	CreateLightingPassRootSignature();
}


void Renderer::CreateGeometryPassRootSignature()
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
		IID_PPV_ARGS(&m_geometryPassRootSignature)));
}


void Renderer::CreateLightingPassRootSignature()
{
	D3D12_DESCRIPTOR_RANGE descriptorRanges[2] = {};
	descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descriptorRanges[0].NumDescriptors = 1;
	descriptorRanges[0].BaseShaderRegister = 0;
	descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	descriptorRanges[0].RegisterSpace = 0;

	descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRanges[1].NumDescriptors = GBuffer::kRtCount;
	descriptorRanges[1].BaseShaderRegister = 0;
	descriptorRanges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	descriptorRanges[1].RegisterSpace = 0;

	D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable = {};
	descriptorTable.NumDescriptorRanges = 2;
	descriptorTable.pDescriptorRanges = descriptorRanges;

	D3D12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].DescriptorTable = descriptorTable;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
	staticSamplers[0].Init(0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	                       D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	staticSamplers[1].Init(1, D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
	                       D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(_countof(rootParameters), rootParameters, static_cast<uint32_t>(std::size(staticSamplers)),
	                       staticSamplers,
	                       D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	DxVerify(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature,
		&error));
	DxVerify(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
		IID_PPV_ARGS(&m_lightingPassRootSignature)));
}


void Renderer::CreatePipelineStateObjects()
{
	CreateGeometryPassPso();
	CreateLightingPassPso();
}


void Renderer::CreateGeometryPassPso()
{
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	uint32_t compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
#else
	uint32_t compileFlags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
#endif

	DxVerify(D3DCompileFromFile(L"Shaders/Deferred/GeometryPass_vs.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"vs_main",
		"vs_5_0",
		compileFlags, 0, &vertexShader, nullptr));
	DxVerify(D3DCompileFromFile(L"Shaders/Deferred/GeometryPass_ps.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps_main",
		"ps_5_0",
		compileFlags, 0, &pixelShader, nullptr));

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{
			"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.InputLayout = {inputElementDescs, _countof(inputElementDescs)};
	psoDesc.pRootSignature = m_geometryPassRootSignature.Get();
	psoDesc.VS = {static_cast<uint8_t*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize()};
	psoDesc.PS = {static_cast<uint8_t*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize()};

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.FrontCounterClockwise = true;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	psoDesc.DepthStencilState.DepthEnable = true;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.StencilEnable = true;
	psoDesc.DepthStencilState.StencilReadMask = 0xff;
	psoDesc.DepthStencilState.StencilWriteMask = 0xff;
	psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_REPLACE;
	psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	psoDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	psoDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_REPLACE;
	psoDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;

	psoDesc.DSVFormat = kDsFormat;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	psoDesc.NumRenderTargets = 4;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.RTVFormats[2] = DXGI_FORMAT_R16G16B16A16_FLOAT;
	psoDesc.RTVFormats[3] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;

	DxVerify(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_geometryPassPipelineStateObject)));
}


// TODO Recompile pixel shader for exactly count of light sources in the scene.
// How to add sources dynamically in this case?
void Renderer::CreateLightingPassPso()
{
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	uint32_t compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
#else
	uint32_t compileFlags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
#endif

	DxVerify(D3DCompileFromFile(L"Shaders/Deferred/LightingPass_vs.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"vs_main",
		"vs_5_0",
		compileFlags, 0, &vertexShader, nullptr));
	DxVerify(D3DCompileFromFile(L"Shaders/Deferred/LightingPass_ps.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"ps_main",
		"ps_5_0",
		compileFlags, 0, &pixelShader, nullptr));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.InputLayout = {nullptr, 0};
	psoDesc.pRootSignature = m_lightingPassRootSignature.Get();
	psoDesc.VS = {static_cast<uint8_t*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize()};
	psoDesc.PS = {static_cast<uint8_t*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize()};

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.FrontCounterClockwise = true;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.DepthStencilState.StencilEnable = true;
	psoDesc.DepthStencilState.StencilReadMask = 0xff;
	psoDesc.DepthStencilState.StencilWriteMask = 0xff;
	psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	psoDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	psoDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;

	psoDesc.DSVFormat = kDsFormat;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = kSwapChainFormat;
	psoDesc.SampleDesc.Count = 1;

	DxVerify(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_lightingPassPipelineStateObject)));
}


void Renderer::CreateCommandList()
{
	// TODO Device4::CreateCommandList1 - creates closed list
	DxVerify(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(),
		m_geometryPassPipelineStateObject.Get(), IID_PPV_ARGS(&m_commandList)));
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
	heapDesc.NumDescriptors = kSwapChainBuffersCount * m_scene->GetSceneObjectsCount() + kSwapChainBuffersCount *
		(1 + GBuffer::kRtCount);
	// plus one for LightSources CBV
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	DxVerify(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));

	auto descriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
	m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
	{
		const auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		assert(
			1024 * 64 >= m_scene->GetSceneObjectsCount() * GeometryPassObjectConstantBuffer::GetAlignedSize() +
			LightingPassConstantBuffer::
			GetAlignedSize());
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
			cbvDesc.SizeInBytes = GeometryPassObjectConstantBuffer::GetAlignedSize();
			m_device->CreateConstantBufferView(&cbvDesc, descriptorHandle);

			descriptorHandle.Offset(1, m_cbvSrvUavDescriptorSize);
			cbAddress += GeometryPassObjectConstantBuffer::GetAlignedSize();
		}
	}

	for (uint32_t i = 0; i < kSwapChainBuffersCount; i++)
	{
		const auto cbAddress = m_constantBufferUploadHeaps[i]->GetGPUVirtualAddress() + m_scene->GetSceneObjectsCount()
			*
			GeometryPassObjectConstantBuffer::GetAlignedSize();

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = LightingPassConstantBuffer::GetAlignedSize();
		m_device->CreateConstantBufferView(&cbvDesc, descriptorHandle);
		descriptorHandle.Offset(1, m_cbvSrvUavDescriptorSize);

		m_device->CreateShaderResourceView(m_gBuffer.m_surfaceColorRt.Get(), nullptr, descriptorHandle);
		descriptorHandle.Offset(1, m_cbvSrvUavDescriptorSize);
		m_device->CreateShaderResourceView(m_gBuffer.m_positionRoughnessRt.Get(), nullptr, descriptorHandle);
		descriptorHandle.Offset(1, m_cbvSrvUavDescriptorSize);
		m_device->CreateShaderResourceView(m_gBuffer.m_normalMetalnessRt.Get(), nullptr, descriptorHandle);
		descriptorHandle.Offset(1, m_cbvSrvUavDescriptorSize);
		m_device->CreateShaderResourceView(m_gBuffer.m_fresnelIndicesRt.Get(), nullptr, descriptorHandle);
		descriptorHandle.Offset(1, m_cbvSrvUavDescriptorSize);
	}
}


// Q: Is it effective to have cbDataCPU? 
void Renderer::UpdateData(float appAspect)
{
	if (!m_cbDataCPU)
		m_cbDataCPU = static_cast<uint8_t*>(malloc(
			GeometryPassObjectConstantBuffer::GetAlignedSize() * m_scene->GetSceneObjectsCount() +
			LightingPassConstantBuffer::GetAlignedSize()));

	auto currentCbPointer = m_cbDataCPU;
	// SceneObjectData
	for (uint32_t i = 0; i < m_scene->GetSceneObjects().size(); i++)
	{
		new(currentCbPointer) GeometryPassObjectConstantBuffer();
		auto& sceneObjectData = *reinterpret_cast<GeometryPassObjectConstantBuffer*>(currentCbPointer);

		sceneObjectData.model = m_scene->GetSceneObjects()[i].GetTransformMatrix();
		sceneObjectData.view = m_scene->GetCamera().GetViewMatrix();
		sceneObjectData.projection = m_scene->GetCamera().GetProjectionMatrix(appAspect);

		const XMMATRIX modelMatrix = XMLoadFloat4x4(&sceneObjectData.model);
		const XMMATRIX viewMatrix = XMLoadFloat4x4(&sceneObjectData.view);
		XMMATRIX projectionMatrix = XMLoadFloat4x4(&sceneObjectData.projection);

		XMMATRIX vpMatrix = XMMatrixMultiply(viewMatrix, projectionMatrix);

		XMStoreFloat4x4(&sceneObjectData.vp, vpMatrix);
		XMStoreFloat4x4(&sceneObjectData.mvp, XMMatrixMultiply(modelMatrix, vpMatrix));

		currentCbPointer += GeometryPassObjectConstantBuffer::GetAlignedSize();
	}

	// LightingPassConstantBuffer
	{
		new(currentCbPointer) LightingPassConstantBuffer();
		auto& lightingPassData = *reinterpret_cast<LightingPassConstantBuffer*>(currentCbPointer);

		const auto cameraPositionVector3 = m_scene->GetCamera().GetPosition();
		lightingPassData.cameraPosition = XMFLOAT4(cameraPositionVector3.x, cameraPositionVector3.y,
		                                           cameraPositionVector3.z, 1.0f);

		lightingPassData.lightSources = m_scene->GetLightSources();

		currentCbPointer += LightingPassConstantBuffer::GetAlignedSize();
	}

	uint8_t* cbDataGpu;
	const auto readRange = CD3DX12_RANGE(0, 0);
	DxVerify(m_constantBufferUploadHeaps[m_frameIndex]->Map(0, &readRange, reinterpret_cast<void**>(&cbDataGpu)));
	memcpy(cbDataGpu, m_cbDataCPU,
	       GeometryPassObjectConstantBuffer::GetAlignedSize() * m_scene->GetSceneObjectsCount() +
	       LightingPassConstantBuffer::GetAlignedSize());
	m_constantBufferUploadHeaps[m_frameIndex]->Unmap(0, nullptr);
}


void Renderer::PopulateCommandList(D3D12_VIEWPORT viewport) const
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


void Renderer::AddGeometryPass(ID3D12GraphicsCommandList* commandList) const
{
	commandList->SetPipelineState(m_geometryPassPipelineStateObject.Get());
	commandList->SetGraphicsRootSignature(m_geometryPassRootSignature.Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = {m_descriptorHeap.Get()};
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	auto cbDescriptorTable = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
	cbDescriptorTable.Offset(m_frameIndex * m_scene->GetSceneObjectsCount(), m_cbvSrvUavDescriptorSize);

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

	commandList->OMSetStencilRef(kGeometryStencilRef);

	for (auto& sceneObject : m_scene->GetSceneObjects())
	{
		commandList->SetGraphicsRootDescriptorTable(0, cbDescriptorTable);
		cbDescriptorTable.Offset(1, m_cbvSrvUavDescriptorSize);

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &sceneObject.GetVertexBufferView());
		commandList->IASetIndexBuffer(&sceneObject.GetIndexBufferView());

		commandList->DrawIndexedInstanced(sceneObject.GetIndicesCount(), 1, 0, 0, 0);
	}
}


void Renderer::AddLightingPass(ID3D12GraphicsCommandList* commandList) const
{
	commandList->SetPipelineState(m_lightingPassPipelineStateObject.Get());

	commandList->SetGraphicsRootSignature(m_lightingPassRootSignature.Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = {m_descriptorHeap.Get()};
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	auto cbDescriptorTable = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
	cbDescriptorTable.Offset(
		kSwapChainBuffersCount * m_scene->GetSceneObjectsCount() + m_frameIndex * (1 + GBuffer::kRtCount),
		m_cbvSrvUavDescriptorSize);
	commandList->SetGraphicsRootDescriptorTable(0, cbDescriptorTable);

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

	commandList->OMSetStencilRef(kGeometryStencilRef);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(6, 1, 0, 0);
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
