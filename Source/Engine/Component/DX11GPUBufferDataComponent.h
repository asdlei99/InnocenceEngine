#pragma once
#include "GPUBufferDataComponent.h"
#include "../RenderingBackend/DX11RenderingBackend/DX11Headers.h"

class DX11GPUBufferDataComponent : public GPUBufferDataComponent
{
public:
	D3D11_BUFFER_DESC m_BufferDesc = {};
	ID3D11Buffer* m_BufferPtr = 0;
	ID3D11ShaderResourceView* m_SRV = 0;
	ID3D11UnorderedAccessView* m_UAV = 0;
};