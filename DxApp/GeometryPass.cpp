#include "GeometryPass.h"

#include <d3dcompiler.h>

#include "RendererForwards.h"
#include "DxHelpers.h"
#include "GBuffer.h"
#include "GeometryPassObjectConstantBuffer.h"
#include "Scene.h"


using namespace Microsoft::WRL;


GeometryPass::GeometryPass(ID3D12Device* device)
{
	Initialize(device);
}


void GeometryPass::Initialize(ID3D12Device* device)
{
	CreateRootSignature(device);
	CreatePipelineStateObject(device);

	m_cbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}


void GeometryPass::SetScene(Scene* scene)
{
	m_scene = scene;
}


void GeometryPass::SetupRootResourceDescriptors(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE rootParameters,
                                                D3D12_GPU_VIRTUAL_ADDRESS dataAddress) const
{
	for (uint32_t j = 0; j < m_scene->GetSceneObjectsCount(); j++)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = dataAddress;
		cbvDesc.SizeInBytes = GeometryPassObjectConstantBuffer::GetAlignedSize();
		device->CreateConstantBufferView(&cbvDesc, rootParameters);

		rootParameters.Offset(1, m_cbvSrvUavDescriptorSize);
		dataAddress += GeometryPassObjectConstantBuffer::GetAlignedSize();
	}
}


void GeometryPass::UpdateRootResources(uint8_t* cbData, float appAspect) const
{
	for (uint32_t i = 0; i < m_scene->GetSceneObjects().size(); i++)
	{
		new(cbData) GeometryPassObjectConstantBuffer();
		auto& sceneObjectData = *reinterpret_cast<GeometryPassObjectConstantBuffer*>(cbData);

		sceneObjectData.model = m_scene->GetSceneObjects()[i].GetTransformMatrix();
		sceneObjectData.view = m_scene->GetCamera().GetViewMatrix();
		sceneObjectData.projection = m_scene->GetCamera().GetProjectionMatrix(appAspect);

		const XMMATRIX modelMatrix = XMLoadFloat4x4(&sceneObjectData.model);
		const XMMATRIX viewMatrix = XMLoadFloat4x4(&sceneObjectData.view);
		XMMATRIX projectionMatrix = XMLoadFloat4x4(&sceneObjectData.projection);

		XMMATRIX vpMatrix = XMMatrixMultiply(viewMatrix, projectionMatrix);

		XMStoreFloat4x4(&sceneObjectData.vp, vpMatrix);
		XMStoreFloat4x4(&sceneObjectData.mvp, XMMatrixMultiply(modelMatrix, vpMatrix));

		cbData += GeometryPassObjectConstantBuffer::GetAlignedSize();
	}
}


void GeometryPass::Setup(ID3D12GraphicsCommandList* commandList) const
{
	commandList->SetPipelineState(m_pipelineStateObject.Get());
	commandList->SetGraphicsRootSignature(m_rootSignature.Get());
}


void GeometryPass::Draw(ID3D12GraphicsCommandList* commandList, CD3DX12_GPU_DESCRIPTOR_HANDLE rootParameters, Scene* scene) const
{
	commandList->OMSetStencilRef(kGeometryStencilRef);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for (auto& sceneObject : scene->GetSceneObjects())
	{
		commandList->SetGraphicsRootDescriptorTable(0, rootParameters);
		rootParameters.Offset(1, m_cbvSrvUavDescriptorSize);

		commandList->IASetVertexBuffers(0, 1, &sceneObject.GetVertexBufferView());
		commandList->IASetIndexBuffer(&sceneObject.GetIndexBufferView());

		commandList->DrawIndexedInstanced(sceneObject.GetIndicesCount(), 1, 0, 0, 0);
	}
}


uint32_t GeometryPass::GetDescriptorTablesDescriptorsCount() const
{
	return m_scene->GetSceneObjectsCount();
}


uint32_t GeometryPass::GetRootResourcesSize() const
{
	return GeometryPassObjectConstantBuffer::GetAlignedSize() * m_scene->GetSceneObjectsCount();
}


void GeometryPass::CreateRootSignature(ID3D12Device* device)
{
	D3D12_DESCRIPTOR_RANGE descriptorRanges[1] = {};
	descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descriptorRanges[0].NumDescriptors = 1;
	descriptorRanges[0].BaseShaderRegister = 0;
	descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	descriptorRanges[0].RegisterSpace = 0;

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
	DxVerify(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
		IID_PPV_ARGS(&m_rootSignature)));
}


void GeometryPass::CreatePipelineStateObject(ID3D12Device* device)
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

	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = { static_cast<uint8_t*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
	psoDesc.PS = { static_cast<uint8_t*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };

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

	psoDesc.NumRenderTargets = GBuffer::kRtCount;
	memcpy(psoDesc.RTVFormats, GBuffer::kRtFormats, sizeof(DXGI_FORMAT) * GBuffer::kRtCount);
	psoDesc.SampleDesc.Count = 1;

	DxVerify(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineStateObject)));
}
