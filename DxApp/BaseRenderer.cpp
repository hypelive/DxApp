#include "dxgi.h"
#include "DxHelpers.h"
#include "BaseRenderer.h"


using namespace DXHelper;

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
		if (SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			*adapter = pAdapter;
			return;
		}
		pAdapter->Release();
	}
}


BaseRenderer::BaseRenderer(HWND hwnd)
{
	// Debug layers
	{
		ID3D12Debug* debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}

	// Device
	{
		IDXGIFactory* factory;
		ThrowIfFailed(CreateDXGIFactory(IID_PPV_ARGS(&factory)));

		IDXGIAdapter* hardwareAdapter;
		GetHardwareAdapter(factory, &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(hardwareAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
	}

	// Command queue
	{
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue)));
	}
}
