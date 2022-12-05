#pragma once

#include "d3d12.h"
#include "d3d12sdklayers.h"


class BaseRenderer
{
public:
	BaseRenderer() = delete;
	BaseRenderer(HWND hwnd);

private:
	ID3D12Device* m_device = nullptr;
	ID3D12CommandQueue* m_commandQueue = nullptr;
	
};

