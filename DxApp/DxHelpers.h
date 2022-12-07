#pragma once

#include <exception>
#include <stdio.h>

namespace DXHelper
{
    // Helper class for COM exceptions
    class ComException : public std::exception
    {
    public:
        ComException(HRESULT hr) : result(hr) {}

        virtual const char* what() const override
        {
            static char s_str[64] = {};
            sprintf_s(s_str, "Failure with HRESULT of %08X",
                static_cast<unsigned int>(result));
            return s_str;
        }

    private:
        HRESULT result;
    };

    // Helper utility converts D3D API failures into exceptions.
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw ComException(hr);
        }
    }
}
