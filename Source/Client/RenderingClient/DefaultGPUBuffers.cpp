#include "DefaultGPUBuffers.h"

#include "../../Engine/ModuleManager/IModuleManager.h"

INNO_ENGINE_API extern IModuleManager* g_pModuleManager;

namespace DefaultGPUBuffers
{
	GPUBufferDataComponent* m_CameraGBDC;
	GPUBufferDataComponent* m_MeshGBDC;
	GPUBufferDataComponent* m_MaterialGBDC;
	GPUBufferDataComponent* m_SunGBDC;
	GPUBufferDataComponent* m_PointLightGBDC;
	GPUBufferDataComponent* m_SphereLightGBDC;
	GPUBufferDataComponent* m_CSMGBDC;
	GPUBufferDataComponent* m_SkyGBDC;
	GPUBufferDataComponent* m_dispatchParamsGBDC;
	GPUBufferDataComponent* m_billboardGBDC;

	std::vector<DispatchParamsGPUData> m_DispatchParamsGPUData;
	std::vector<MeshGPUData> m_billboardGPUData;
}

bool DefaultGPUBuffers::Setup()
{
	return true;
}

bool DefaultGPUBuffers::Initialize()
{
	auto l_RenderingCapability = g_pModuleManager->getRenderingFrontend()->getRenderingCapability();

	m_CameraGBDC = g_pModuleManager->getRenderingServer()->AddGPUBufferDataComponent("CameraGPUBuffer/");
	m_CameraGBDC->m_ElementCount = 1;
	m_CameraGBDC->m_ElementSize = sizeof(CameraGPUData);
	m_CameraGBDC->m_BindingPoint = 0;

	g_pModuleManager->getRenderingServer()->InitializeGPUBufferDataComponent(m_CameraGBDC);

	m_MeshGBDC = g_pModuleManager->getRenderingServer()->AddGPUBufferDataComponent("MeshGPUBuffer/");
	m_MeshGBDC->m_ElementCount = l_RenderingCapability.maxMeshes;
	m_MeshGBDC->m_ElementSize = sizeof(MeshGPUData);
	m_MeshGBDC->m_BindingPoint = 1;

	g_pModuleManager->getRenderingServer()->InitializeGPUBufferDataComponent(m_MeshGBDC);

	m_MaterialGBDC = g_pModuleManager->getRenderingServer()->AddGPUBufferDataComponent("MaterialGPUBuffer/");
	m_MaterialGBDC->m_ElementCount = l_RenderingCapability.maxMaterials;
	m_MaterialGBDC->m_ElementSize = sizeof(MaterialGPUData);
	m_MaterialGBDC->m_BindingPoint = 2;

	g_pModuleManager->getRenderingServer()->InitializeGPUBufferDataComponent(m_MaterialGBDC);

	m_SunGBDC = g_pModuleManager->getRenderingServer()->AddGPUBufferDataComponent("SunGPUBuffer/");
	m_SunGBDC->m_ElementCount = 1;
	m_SunGBDC->m_ElementSize = sizeof(SunGPUData);
	m_SunGBDC->m_BindingPoint = 3;

	g_pModuleManager->getRenderingServer()->InitializeGPUBufferDataComponent(m_SunGBDC);

	m_PointLightGBDC = g_pModuleManager->getRenderingServer()->AddGPUBufferDataComponent("PointLightGPUBuffer/");
	m_PointLightGBDC->m_ElementCount = l_RenderingCapability.maxPointLights;
	m_PointLightGBDC->m_ElementSize = sizeof(PointLightGPUData);
	m_PointLightGBDC->m_BindingPoint = 4;

	g_pModuleManager->getRenderingServer()->InitializeGPUBufferDataComponent(m_PointLightGBDC);

	m_SphereLightGBDC = g_pModuleManager->getRenderingServer()->AddGPUBufferDataComponent("SphereLightGPUBuffer/");
	m_SphereLightGBDC->m_ElementCount = l_RenderingCapability.maxSphereLights;
	m_SphereLightGBDC->m_ElementSize = sizeof(SphereLightGPUData);
	m_SphereLightGBDC->m_BindingPoint = 5;

	g_pModuleManager->getRenderingServer()->InitializeGPUBufferDataComponent(m_SphereLightGBDC);

	m_CSMGBDC = g_pModuleManager->getRenderingServer()->AddGPUBufferDataComponent("CSMGPUBuffer/");
	m_CSMGBDC->m_ElementCount = l_RenderingCapability.maxCSMSplits;
	m_CSMGBDC->m_ElementSize = sizeof(CSMGPUData);
	m_CSMGBDC->m_BindingPoint = 6;

	g_pModuleManager->getRenderingServer()->InitializeGPUBufferDataComponent(m_CSMGBDC);

	m_SkyGBDC = g_pModuleManager->getRenderingServer()->AddGPUBufferDataComponent("SkyGPUBuffer/");
	m_SkyGBDC->m_ElementCount = 1;
	m_SkyGBDC->m_ElementSize = sizeof(SkyGPUData);
	m_SkyGBDC->m_BindingPoint = 7;

	g_pModuleManager->getRenderingServer()->InitializeGPUBufferDataComponent(m_SkyGBDC);

	// @TODO: get rid of hard-code stuffs
	m_dispatchParamsGBDC = g_pModuleManager->getRenderingServer()->AddGPUBufferDataComponent("DispatchParamsGPUBuffer/");
	m_dispatchParamsGBDC->m_ElementCount = 8;
	m_dispatchParamsGBDC->m_ElementSize = sizeof(DispatchParamsGPUData);
	m_dispatchParamsGBDC->m_BindingPoint = 8;

	g_pModuleManager->getRenderingServer()->InitializeGPUBufferDataComponent(m_dispatchParamsGBDC);

	m_billboardGBDC = g_pModuleManager->getRenderingServer()->AddGPUBufferDataComponent("BillboardGPUBuffer/");
	m_billboardGBDC->m_ElementCount = l_RenderingCapability.maxMeshes;
	m_billboardGBDC->m_ElementSize = sizeof(MeshGPUData);
	m_billboardGBDC->m_BindingPoint = 12;

	g_pModuleManager->getRenderingServer()->InitializeGPUBufferDataComponent(m_billboardGBDC);

	m_billboardGPUData.resize(l_RenderingCapability.maxMeshes);

	return true;
}

bool DefaultGPUBuffers::Upload()
{
	auto l_CameraGPUData = g_pModuleManager->getRenderingFrontend()->getCameraGPUData();
	auto l_MeshGPUData = g_pModuleManager->getRenderingFrontend()->getOpaquePassMeshGPUData();
	auto l_MaterialGPUData = g_pModuleManager->getRenderingFrontend()->getOpaquePassMaterialGPUData();
	auto l_SunGPUData = g_pModuleManager->getRenderingFrontend()->getSunGPUData();
	auto l_PointLightGPUData = g_pModuleManager->getRenderingFrontend()->getPointLightGPUData();
	auto l_SphereLightGPUData = g_pModuleManager->getRenderingFrontend()->getSphereLightGPUData();
	auto l_CSMGPUData = g_pModuleManager->getRenderingFrontend()->getCSMGPUData();
	auto l_SkyGPUData = g_pModuleManager->getRenderingFrontend()->getSkyGPUData();

	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(m_CameraGBDC, &l_CameraGPUData);
	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(m_MeshGBDC, l_MeshGPUData);
	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(m_MaterialGBDC, l_MaterialGPUData);
	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(m_SunGBDC, &l_SunGPUData);
	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(m_PointLightGBDC, l_PointLightGPUData);
	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(m_SphereLightGBDC, l_SphereLightGPUData);
	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(m_CSMGBDC, l_CSMGPUData);
	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(m_SkyGBDC, &l_SkyGPUData);

	auto l_drawCallCount = g_pModuleManager->getRenderingFrontend()->getBillboardPassDrawCallCount();
	auto l_billboardPassGPUData = g_pModuleManager->getRenderingFrontend()->getBillboardPassGPUData();

	for (unsigned int i = 0; i < l_drawCallCount; i++)
	{
		m_billboardGPUData[i].m = l_billboardPassGPUData[i].m;
	}

	g_pModuleManager->getRenderingServer()->UploadGPUBufferDataComponent(m_billboardGBDC, m_billboardGPUData);

	return true;
}

bool DefaultGPUBuffers::Terminate()
{
	g_pModuleManager->getRenderingServer()->DeleteGPUBufferDataComponent(m_CameraGBDC);
	g_pModuleManager->getRenderingServer()->DeleteGPUBufferDataComponent(m_MeshGBDC);
	g_pModuleManager->getRenderingServer()->DeleteGPUBufferDataComponent(m_MaterialGBDC);
	g_pModuleManager->getRenderingServer()->DeleteGPUBufferDataComponent(m_SunGBDC);;
	g_pModuleManager->getRenderingServer()->DeleteGPUBufferDataComponent(m_PointLightGBDC);
	g_pModuleManager->getRenderingServer()->DeleteGPUBufferDataComponent(m_SphereLightGBDC);
	g_pModuleManager->getRenderingServer()->DeleteGPUBufferDataComponent(m_CSMGBDC);
	g_pModuleManager->getRenderingServer()->DeleteGPUBufferDataComponent(m_SkyGBDC);
	g_pModuleManager->getRenderingServer()->DeleteGPUBufferDataComponent(m_dispatchParamsGBDC);
	g_pModuleManager->getRenderingServer()->DeleteGPUBufferDataComponent(m_billboardGBDC);

	return true;
}

GPUBufferDataComponent * DefaultGPUBuffers::GetGPUBufferDataComponent(GPUBufferUsageType usageType)
{
	GPUBufferDataComponent* l_result;

	switch (usageType)
	{
	case GPUBufferUsageType::Camera: l_result = m_CameraGBDC;
		break;
	case GPUBufferUsageType::Mesh: l_result = m_MeshGBDC;
		break;
	case GPUBufferUsageType::Material: l_result = m_MaterialGBDC;
		break;
	case GPUBufferUsageType::Sun: l_result = m_SunGBDC;
		break;
	case GPUBufferUsageType::PointLight: l_result = m_PointLightGBDC;
		break;
	case GPUBufferUsageType::SphereLight: l_result = m_SphereLightGBDC;
		break;
	case GPUBufferUsageType::CSM: l_result = m_CSMGBDC;
		break;
	case GPUBufferUsageType::Sky: l_result = m_SkyGBDC;
		break;
	case GPUBufferUsageType::Compute: l_result = m_dispatchParamsGBDC;
		break;
	case GPUBufferUsageType::SH9:
		break;
	case GPUBufferUsageType::Billboard: l_result = m_billboardGBDC;
		break;
	default:
		break;
	}

	return l_result;
}