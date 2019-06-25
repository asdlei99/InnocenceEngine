#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include "DX12/d3dx12.h"

struct DX12ConstantBuffer
{
	ID3D12Resource* m_constantBuffer = 0;
	void* mappedMemory = 0;
	size_t elementCount = 0;
	size_t elementSize = 0;
};

struct DX12CBV
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC CBVDesc = {};
	D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle;
};