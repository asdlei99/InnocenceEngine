#pragma once
#include "../../Common/InnoType.h"

#include "../../Component/DX12MeshDataComponent.h"
#include "../../Component/DX12TextureDataComponent.h"
#include "../../Component/DX12MaterialDataComponent.h"
#include "../../Component/DX12ShaderProgramComponent.h"
#include "../../Component/DX12RenderPassComponent.h"

namespace DX12RenderingBackendNS
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
	DX12MaterialDataComponent* addDX12MaterialDataComponent();
	DX12TextureDataComponent* addDX12TextureDataComponent();

	DX12MeshDataComponent* getDX12MeshDataComponent(MeshShapeType MeshShapeType);
	DX12TextureDataComponent* getDX12TextureDataComponent(TextureUsageType TextureUsageType);
	DX12TextureDataComponent* getDX12TextureDataComponent(FileExplorerIconType iconType);
	DX12TextureDataComponent* getDX12TextureDataComponent(WorldEditorIconType iconType);
	DX12MaterialDataComponent* getDefaultMaterialDataComponent();

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

	bool destroyDX12RenderPassComponent(DX12RenderPassComponent* DXRPC);

	bool initializeDX12MeshDataComponent(DX12MeshDataComponent* rhs);
	bool initializeDX12TextureDataComponent(DX12TextureDataComponent* rhs);
	DX12SRV createSRV(const DX12TextureDataComponent& rhs);
	bool initializeDX12MaterialDataComponent(DX12MaterialDataComponent* rhs);

	bool destroyAllGraphicPrimitiveComponents();

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

	bool recordCommandBegin(DX12RenderPassComponent* DXRPC, uint32_t frameIndex);
	bool recordActivateRenderPass(DX12RenderPassComponent* DXRPC, uint32_t frameIndex);
	bool recordBindDescHeaps(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, uint32_t heapsCount, ID3D12DescriptorHeap** heaps);
	bool recordBindCBV(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, uint32_t startSlot, const DX12ConstantBuffer& ConstantBuffer, size_t offset);
	bool recordBindRTForWrite(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, DX12TextureDataComponent* DXTDC);
	bool recordBindRTForRead(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, DX12TextureDataComponent* DXTDC);
	bool recordBindSRVDescTable(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, uint32_t startSlot, const DX12SRV& SRV);
	bool recordBindSamplerDescTable(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, uint32_t startSlot, DX12ShaderProgramComponent* DXSPC);
	bool recordDrawCall(DX12RenderPassComponent* DXRPC, uint32_t frameIndex, DX12MeshDataComponent* DXMDC);
	bool recordCommandEnd(DX12RenderPassComponent* DXRPC, uint32_t frameIndex);
	bool executeCommandList(DX12RenderPassComponent* DXRPC, uint32_t frameIndex);
	bool waitFrame(DX12RenderPassComponent* DXRPC, uint32_t frameIndex);

	DX12ShaderProgramComponent* addDX12ShaderProgramComponent(EntityID rhs);
	bool initializeDX12ShaderProgramComponent(DX12ShaderProgramComponent* rhs, const ShaderFilePaths& shaderFilePaths);
}