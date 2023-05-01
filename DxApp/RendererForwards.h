#pragma once

#include <stdint.h>
#include <dxgiformat.h>


static constexpr DXGI_FORMAT kDsFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
static constexpr DXGI_FORMAT kSwapChainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

static constexpr uint8_t kGeometryStencilRef = 1;