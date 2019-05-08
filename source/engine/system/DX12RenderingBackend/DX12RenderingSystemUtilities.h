#pragma once
#include "../../common/InnoType.h"

#include "../../component/MeshDataComponent.h"
#include "../../component/MaterialDataComponent.h"
#include "../../component/TextureDataComponent.h"
#include "../../component/DX12MeshDataComponent.h"
#include "../../component/DX12TextureDataComponent.h"
#include "../../component/DX12ShaderProgramComponent.h"
#include "../../component/DX12RenderPassComponent.h"

INNO_PRIVATE_SCOPE DX12RenderingSystemNS
{
	bool setup();
	bool initialize();
	bool update();
	bool render();
	bool terminate();
	bool initializeComponentPool();
	bool resize();

	void loadDefaultAssets();
	bool generateGPUBuffers();

	DX12MeshDataComponent* addDX12MeshDataComponent();
	MaterialDataComponent* addMaterialDataComponent();
	DX12TextureDataComponent* addDX12TextureDataComponent();

	DX12MeshDataComponent* getDX12MeshDataComponent(EntityID meshID);
	DX12TextureDataComponent* getDX12TextureDataComponent(EntityID textureID);

	DX12MeshDataComponent* getDX12MeshDataComponent(MeshShapeType MeshShapeType);
	DX12TextureDataComponent* getDX12TextureDataComponent(TextureUsageType TextureUsageType);
	DX12TextureDataComponent* getDX12TextureDataComponent(FileExplorerIconType iconType);
	DX12TextureDataComponent* getDX12TextureDataComponent(WorldEditorIconType iconType);

	DX12RenderPassComponent* addDX12RenderPassComponent(const EntityID& parentEntity, const char* name);
	bool initializeDX12RenderPassComponent(DX12RenderPassComponent* DXRPC, DX12ShaderProgramComponent* DXSPC);

	bool reserveRenderTargets(DX12RenderPassComponent* DXRPC);
	bool createRenderTargets(DX12RenderPassComponent* DXRPC);
	bool createRTVDescriptorHeap(DX12RenderPassComponent* DXRPC);
	bool createRTV(DX12RenderPassComponent* DXRPC);
	bool createDSVDescriptorHeap(DX12RenderPassComponent* DXRPC);
	bool createDSV(DX12RenderPassComponent* DXRPC);
	bool createRootSignature(DX12RenderPassComponent* DXRPC);
	bool createPSO(DX12RenderPassComponent* DXRPC, DX12ShaderProgramComponent* DXSPC);
	bool createCommandQueue(DX12RenderPassComponent* DXRPC);
	bool createCommandAllocators(DX12RenderPassComponent* DXRPC);
	bool createCommandLists(DX12RenderPassComponent* DXRPC);
	bool createSyncPrimitives(DX12RenderPassComponent* DXRPC);

	bool initializeDX12MeshDataComponent(DX12MeshDataComponent* rhs);
	bool initializeDX12TextureDataComponent(DX12TextureDataComponent* rhs);

	DX12ConstantBuffer createConstantBuffer(size_t elementSize, size_t elementCount, const std::wstring& name);
	DX12CBV createCBV(const DX12ConstantBuffer& arg, size_t offset);
	void updateConstantBufferImpl(const DX12ConstantBuffer& ConstantBuffer, size_t size, const void* ConstantBufferValue);

	template <class T>
	void updateConstantBuffer(const DX12ConstantBuffer& ConstantBuffer, const T& ConstantBufferValue)
	{
		updateConstantBufferImpl(ConstantBuffer, sizeof(T), &ConstantBufferValue);
	}

	template <class T>
		void updateConstantBuffer(const DX12ConstantBuffer& ConstantBuffer, const std::vector<T>& ConstantBufferValue)
	{
		updateConstantBufferImpl(ConstantBuffer, sizeof(T) * ConstantBufferValue.size(), &ConstantBufferValue[0]);
	}

	bool recordCommandBegin(DX12RenderPassComponent* DXRPC, unsigned int frameIndex);
	bool recordActivateRenderPass(DX12RenderPassComponent* DXRPC, unsigned int frameIndex);
	bool recordBindDescHeaps(DX12RenderPassComponent* DXRPC, unsigned int frameIndex, unsigned int heapsCount, ID3D12DescriptorHeap** heaps);
	bool recordBindCBV(DX12RenderPassComponent* DXRPC, unsigned int frameIndex, unsigned int startSlot, const DX12ConstantBuffer& ConstantBuffer, size_t offset);
	bool recordBindTextureForWrite(DX12RenderPassComponent* DXRPC, unsigned int frameIndex, DX12TextureDataComponent* DXTDC);
	bool recordBindTextureForRead(DX12RenderPassComponent* DXRPC, unsigned int frameIndex, DX12TextureDataComponent* DXTDC);
	bool recordBindSRV(DX12RenderPassComponent* DXRPC, unsigned int frameIndex, unsigned int startSlot, const DX12TextureDataComponent* DXTDC);
	bool recordBindSRVDescTable(DX12RenderPassComponent* DXRPC, unsigned int frameIndex, unsigned int startSlot, const DX12TextureDataComponent* DXTDC);
	bool recordBindSamplerDescTable(DX12RenderPassComponent* DXRPC, unsigned int frameIndex, unsigned int startSlot, DX12ShaderProgramComponent* DXSPC);
	bool recordDrawCall(DX12RenderPassComponent* DXRPC, unsigned int frameIndex, DX12MeshDataComponent* DXMDC);
	bool recordCommandEnd(DX12RenderPassComponent* DXRPC, unsigned int frameIndex);
	bool executeCommandList(DX12RenderPassComponent* DXRPC, unsigned int frameIndex);
	bool waitFrame(DX12RenderPassComponent* DXRPC, unsigned int frameIndex);

	DX12ShaderProgramComponent* addDX12ShaderProgramComponent(EntityID rhs);
	bool initializeDX12ShaderProgramComponent(DX12ShaderProgramComponent* rhs, const ShaderFilePaths& shaderFilePaths);
}