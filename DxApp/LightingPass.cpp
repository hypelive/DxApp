#include "LightingPass.h"

#include <d3dcompiler.h>

#include "RendererForwards.h"
#include "DxHelpers.h"
#include "GBuffer.h"
#include "GeometryPassObjectConstantBuffer.h"
#include "Scene.h"

namespace
{
	struct LightingPassConstantBuffer
	{
		XMFLOAT4 cameraPosition;
		LightSources lightSources;

		static uint32_t GetAlignedSize() { return (sizeof(LightingPassConstantBuffer) + 255) & ~255; }
	};
}


LightingPass::LightingPass(ID3D12Device* device)
{
	Initialize(device);
}


void LightingPass::Initialize(ID3D12Device* device)
{
	CreateRootSignature(device);
	CreatePipelineStateObject(device);

	m_cbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}


void LightingPass::SetScene(Scene* scene)
{
	m_scene = scene;
}


void LightingPass::SetupRootResourceDescriptors(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE rootParameters,
	D3D12_GPU_VIRTUAL_ADDRESS dataAddress, const GBuffer& gBuffer) const
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = dataAddress;
	cbvDesc.SizeInBytes = LightingPassConstantBuffer::GetAlignedSize();
	device->CreateConstantBufferView(&cbvDesc, rootParameters);
	rootParameters.Offset(1, m_cbvSrvUavDescriptorSize);

	device->CreateShaderResourceView(gBuffer.m_surfaceColorRt.Get(), nullptr, rootParameters);
	rootParameters.Offset(1, m_cbvSrvUavDescriptorSize);
	device->CreateShaderResourceView(gBuffer.m_positionRoughnessRt.Get(), nullptr, rootParameters);
	rootParameters.Offset(1, m_cbvSrvUavDescriptorSize);
	device->CreateShaderResourceView(gBuffer.m_normalMetalnessRt.Get(), nullptr, rootParameters);
	rootParameters.Offset(1, m_cbvSrvUavDescriptorSize);
	device->CreateShaderResourceView(gBuffer.m_fresnelIndicesRt.Get(), nullptr, rootParameters);
	rootParameters.Offset(1, m_cbvSrvUavDescriptorSize);
}


void LightingPass::UpdateRootResources(uint8_t* cbData) const
{
	new(cbData) LightingPassConstantBuffer();
	auto& lightingPassData = *reinterpret_cast<LightingPassConstantBuffer*>(cbData);

	const auto cameraPositionVector3 = m_scene->GetCamera().GetPosition();
	lightingPassData.cameraPosition = XMFLOAT4(cameraPositionVector3.x, cameraPositionVector3.y,
		cameraPositionVector3.z, 1.0f);

	lightingPassData.lightSources = m_scene->GetLightSources();
}


void LightingPass::Setup(ID3D12GraphicsCommandList* commandList) const
{
	commandList->SetPipelineState(m_pipelineStateObject.Get());
	commandList->SetGraphicsRootSignature(m_rootSignature.Get());
}


// TODO draw triangle
void LightingPass::Draw(ID3D12GraphicsCommandList* commandList, CD3DX12_GPU_DESCRIPTOR_HANDLE rootParameters) const
{
	commandList->SetGraphicsRootDescriptorTable(0, rootParameters);

	commandList->OMSetStencilRef(kGeometryStencilRef);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(6, 1, 0, 0);
}


uint32_t LightingPass::GetDescriptorTablesDescriptorsCount() const
{
	return 1 + GBuffer::kRtCount;
}


uint32_t LightingPass::GetRootResourcesSize() const
{
	return LightingPassConstantBuffer::GetAlignedSize();
}


void LightingPass::CreateRootSignature(ID3D12Device* device)
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
	DxVerify(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
		IID_PPV_ARGS(&m_rootSignature)));
}


void LightingPass::CreatePipelineStateObject(ID3D12Device* device)
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

	psoDesc.InputLayout = { nullptr, 0 };
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = { static_cast<uint8_t*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
	psoDesc.PS = { static_cast<uint8_t*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };

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

	DxVerify(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateObject)));
}
