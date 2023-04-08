#pragma once

#include <intrin.h>
#include <iostream>
#include <d3dx12.h>


#ifdef _DEBUG
#define DxVerify(hr) {					\
		if (FAILED(hr))					\
		{								\
		    std::cout << (hr) << "\n";	\
			__debugbreak();				\
		}								\
	}
#else 
#define DxVerify(hr) {					\
		if (FAILED(hr))					\
		{								\
		    std::cout << (hr) << "\n";	\
		}								\
	}
#endif


namespace DxHelper
{
	template <uint32_t TSize>
	using ResourceBarriersArray = std::array<CD3DX12_RESOURCE_BARRIER, TSize>;

	inline uint32_t Align(const uint32_t size, const uint32_t alignment)
	{
		const uint32_t temp = alignment - 1;
		return (size + temp) & ~temp;
	}

	inline void SetRenderTarget(ID3D12GraphicsCommandList* commandList, D3D12_VIEWPORT viewport)
	{
		const D3D12_VIEWPORT viewports[] = { viewport };
		commandList->RSSetViewports(1, viewports);
		const D3D12_RECT scissorsRects[] = {
			{0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height)}
		};
		commandList->RSSetScissorRects(1, scissorsRects);
	}
}
