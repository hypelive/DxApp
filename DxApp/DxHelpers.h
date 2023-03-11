#pragma once

#include <intrin.h>
#include <iostream>


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
	inline uint32_t Align(const uint32_t size, const uint32_t alignment)
	{
		const uint32_t temp = alignment - 1;
		return (size + temp) & ~temp;
	}
}
