#pragma once

#include <stdint.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_4.h>
#include <wrl.h>


class Scene;
class GBuffer;

class LightingPass
{
public:
	LightingPass() = default;
	explicit LightingPass(ID3D12Device* device);
	void Initialize(ID3D12Device* device);
	~LightingPass() = default;

	void SetScene(Scene* scene);
	void SetupRootResourceDescriptors(ID3D12Device* device, CD3DX12_CPU_DESCRIPTOR_HANDLE rootParameters, D3D12_GPU_VIRTUAL_ADDRESS dataAddress, const GBuffer& gBuffer) const;
	void UpdateRootResources(uint8_t* cbData) const;
	void Setup(ID3D12GraphicsCommandList* commandList) const;
	void Draw(ID3D12GraphicsCommandList* commandList, CD3DX12_GPU_DESCRIPTOR_HANDLE rootParameters) const;

	[[nodiscard]] uint32_t GetDescriptorTablesDescriptorsCount() const;
	[[nodiscard]] uint32_t GetRootResourcesSize() const;

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineStateObject;
	Scene* m_scene = nullptr;

	uint32_t m_cbvSrvUavDescriptorSize = 0;

	void CreateRootSignature(ID3D12Device* device);
	void CreatePipelineStateObject(ID3D12Device* device);
};

