#pragma once

#include <intrin.h>

namespace DxHelper
{
	inline void DxVerify(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// Log message

#ifdef _DEBUG
			__debugbreak();
#endif
		}
	}
}
