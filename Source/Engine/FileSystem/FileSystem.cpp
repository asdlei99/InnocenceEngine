#include "FileSystem.h"
#include "../Common/CommonMacro.inl"
#include "../Component/MeshDataComponent.h"
#include "../Component/TextureDataComponent.h"
#include "../ComponentManager/IVisibleComponentManager.h"
#include "../Core/InnoLogger.h"

#include "../ModuleManager/IModuleManager.h"
extern IModuleManager* g_pModuleManager;

#include "../Core/IOService.h"
#include "AssimpWrapper.h"
#include "AssetLoader.h"
#include "JSONParser.h"

using SceneLoadingCallback = std::pair<std::function<void()>*, int>;

namespace InnoFileSystemNS
{
	bool convertModel(const std::string & fileName, const std::string & exportPath);

	bool saveScene(const std::string& fileName);
	bool prepareForLoadingScene(const std::string& fileName);
	bool loadScene(const std::string& fileName);

	ObjectStatus m_objectStatus = ObjectStatus::Terminated;

	std::vector<SceneLoadingCallback> m_sceneLoadingStartCallbacks;
	std::vector<SceneLoadingCallback> m_sceneLoadingFinishCallbacks;

	std::atomic<bool> m_isLoadingScene = false;
	std::atomic<bool> m_prepareForLoadingScene = false;

	std::string m_nextLoadingScene;
	std::string m_currentScene;
}

bool InnoFileSystemNS::convertModel(const std::string & fileName, const std::string & exportPath)
{
	auto l_extension = IOService::getFileExtension(fileName);

	if (l_extension == ".obj" || l_extension == ".OBJ" || l_extension == ".fbx" || l_extension == ".FBX")
	{
		auto tempTask = g_pModuleManager->getTaskSystem()->submit("ConvertModelTask", -1, nullptr, [=]()
		{
			AssimpWrapper::convertModel(fileName, exportPath);
		});
		return true;
	}
	else
	{
		InnoLogger::Log(LogLevel::Warning, "FileSystem: ", fileName.c_str(), " is not supported!");

		return false;
	}

	return true;
}

bool InnoFileSystemNS::saveScene(const std::string& fileName)
{
	if (fileName.empty())
	{
		return JSONParser::saveScene(m_currentScene);
	}
	else
	{
		return JSONParser::saveScene(fileName);
	}
}

bool InnoFileSystemNS::prepareForLoadingScene(const std::string& fileName)
{
	if (!InnoFileSystemNS::m_isLoadingScene)
	{
		if (m_currentScene == fileName)
		{
			InnoLogger::Log(LogLevel::Warning, "FileSystem: scene ", fileName.c_str(), " has already loaded now.");
			return true;
		}
		m_nextLoadingScene = fileName;
		m_prepareForLoadingScene = true;
	}

	return true;
}

bool InnoFileSystemNS::loadScene(const std::string& fileName)
{
	m_currentScene = fileName;

	std::sort(m_sceneLoadingStartCallbacks.begin(), m_sceneLoadingStartCallbacks.end(),
		[&](SceneLoadingCallback A, SceneLoadingCallback B) {
		return A.second > B.second;
	});

	std::sort(m_sceneLoadingFinishCallbacks.begin(), m_sceneLoadingFinishCallbacks.end(),
		[&](SceneLoadingCallback A, SceneLoadingCallback B) {
		return A.second > B.second;
	});

	for (auto& i : m_sceneLoadingStartCallbacks)
	{
		(*i.first)();
	}

	JSONParser::loadScene(fileName);

	for (auto& i : m_sceneLoadingFinishCallbacks)
	{
		(*i.first)();
	}

	InnoFileSystemNS::m_isLoadingScene = false;

	InnoLogger::Log(LogLevel::Success, "FileSystem: scene ", fileName.c_str(), " has been loaded.");

	return true;
}

bool InnoFileSystem::setup()
{
	IOService::setupWorkingDirectory();

	InnoFileSystemNS::m_objectStatus = ObjectStatus::Created;
	return true;
}

bool InnoFileSystem::initialize()
{
	if (InnoFileSystemNS::m_objectStatus == ObjectStatus::Created)
	{
		InnoFileSystemNS::m_objectStatus = ObjectStatus::Activated;
		InnoLogger::Log(LogLevel::Success, "FileSystem has been initialized.");
		return true;
	}
	else
	{
		InnoLogger::Log(LogLevel::Error, "FileSystem: Object is not created!");
		return false;
	}
}

bool InnoFileSystem::update()
{
	if (InnoFileSystemNS::m_objectStatus == ObjectStatus::Activated)
	{
		if (InnoFileSystemNS::m_prepareForLoadingScene)
		{
			InnoFileSystemNS::m_prepareForLoadingScene = false;
			InnoFileSystemNS::m_isLoadingScene = true;
			g_pModuleManager->getTaskSystem()->waitAllTasksToFinish();
			InnoFileSystemNS::loadScene(InnoFileSystemNS::m_nextLoadingScene);
			GetComponentManager(VisibleComponent)->LoadAssetsForComponents();
		}
		return true;
	}
	else
	{
		InnoFileSystemNS::m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	return true;
}

bool InnoFileSystem::terminate()
{
	InnoFileSystemNS::m_objectStatus = ObjectStatus::Terminated;

	return true;
}

ObjectStatus InnoFileSystem::getStatus()
{
	return InnoFileSystemNS::m_objectStatus;
}

std::string InnoFileSystem::getWorkingDirectory()
{
	return IOService::getWorkingDirectory();
}

std::vector<char> InnoFileSystem::loadFile(const std::string& filePath, IOMode openMode)
{
	return IOService::loadFile(filePath, openMode);
}

bool InnoFileSystem::saveFile(const std::string& filePath, const std::vector<char>& content, IOMode saveMode)
{
	return IOService::saveFile(filePath, content, saveMode);
}

std::string InnoFileSystem::getCurrentSceneName()
{
	auto l_currentSceneName = InnoFileSystemNS::m_currentScene.substr(0, InnoFileSystemNS::m_currentScene.find("."));
	l_currentSceneName = l_currentSceneName.substr(l_currentSceneName.rfind("//") + 2);
	return l_currentSceneName;
}

bool InnoFileSystem::loadScene(const std::string & fileName, bool AsyncLoad)
{
	if (InnoFileSystemNS::m_currentScene == fileName)
	{
		InnoLogger::Log(LogLevel::Warning, "FileSystem: scene ", fileName.c_str(), " has already loaded now.");
		return true;
	}

	if (AsyncLoad)
	{
		return InnoFileSystemNS::prepareForLoadingScene(fileName);
	}
	else
	{
		InnoFileSystemNS::m_nextLoadingScene = fileName;
		InnoFileSystemNS::m_isLoadingScene = true;
		g_pModuleManager->getTaskSystem()->waitAllTasksToFinish();
		InnoFileSystemNS::loadScene(InnoFileSystemNS::m_nextLoadingScene);
		GetComponentManager(VisibleComponent)->LoadAssetsForComponents(AsyncLoad);
		return true;
	}
}

bool InnoFileSystem::saveScene(const std::string & fileName)
{
	return InnoFileSystemNS::saveScene(fileName);
}

bool InnoFileSystem::isLoadingScene()
{
	return InnoFileSystemNS::m_isLoadingScene;
}

bool InnoFileSystem::addSceneLoadingStartCallback(std::function<void()>* functor, int priority)
{
	InnoFileSystemNS::m_sceneLoadingStartCallbacks.emplace_back(functor, priority);
	return true;
}

bool InnoFileSystem::addSceneLoadingFinishCallback(std::function<void()>* functor, int priority)
{
	InnoFileSystemNS::m_sceneLoadingFinishCallbacks.emplace_back(functor, priority);
	return true;
}

bool InnoFileSystem::convertModel(const std::string & fileName, const std::string & exportPath)
{
	return InnoFileSystemNS::convertModel(fileName, exportPath);
}

ModelMap InnoFileSystem::loadModel(const std::string & fileName, bool AsyncUploadGPUResource)
{
	return InnoFileSystemNS::AssetLoader::loadModel(fileName, AsyncUploadGPUResource);
}

TextureDataComponent* InnoFileSystem::loadTexture(const std::string & fileName)
{
	return InnoFileSystemNS::AssetLoader::loadTexture(fileName);
}

bool InnoFileSystem::addCPPClassFiles(const CPPClassDesc & desc)
{
	// Build header file
	auto l_headerFileName = desc.filePath + desc.className + ".h";
	std::ofstream l_headerFile(IOService::getWorkingDirectory() + l_headerFileName, std::ios::out | std::ios::trunc);

	if (!l_headerFile.is_open())
	{
		InnoLogger::Log(LogLevel::Error, "FileSystem: std::ofstream: can't open file ", l_headerFileName.c_str(), "!");
		return false;
	}

	// Common headers include
	l_headerFile << "#pragma once" << std::endl;
	l_headerFile << "#include \"Common/InnoType.h\"" << std::endl;
	l_headerFile << "#include \"Common/InnoClassTemplate.h\"" << std::endl;
	l_headerFile << std::endl;

	// Abstraction type
	if (desc.isInterface)
	{
		l_headerFile << "class ";
	}
	else
	{
		l_headerFile << "class ";
	}

	l_headerFile << desc.className;

	// Inheriance type
	if (!desc.parentClass.empty())
	{
		l_headerFile << " : public " << desc.parentClass;
	}

	l_headerFile << std::endl;

	// Class decl body
	l_headerFile << "{" << std::endl;
	l_headerFile << "public:" << std::endl;

	// Ctor type
	if (desc.isInterface)
	{
		if (desc.isNonMoveable && desc.isNonCopyable)
		{
			l_headerFile << "  INNO_CLASS_INTERFACE_NON_COPYABLE_AND_NON_MOVABLE(" << desc.className << ");" << std::endl;
		}
		else if (desc.isNonMoveable)
		{
			l_headerFile << "  INNO_CLASS_INTERFACE_NON_MOVABLE(" << desc.className << ");" << std::endl;
		}
		else if (desc.isNonCopyable)
		{
			l_headerFile << "  INNO_CLASS_INTERFACE_NON_COPYABLE(" << desc.className << ");" << std::endl;
		}
		else
		{
			l_headerFile << "  INNO_CLASS_INTERFACE_DEFALUT(" << desc.className << ");" << std::endl;
		}
	}
	else
	{
		if (desc.isNonMoveable && desc.isNonCopyable)
		{
			l_headerFile << "  INNO_CLASS_CONCRETE_NON_COPYABLE_AND_NON_MOVABLE(" << desc.className << ");" << std::endl;
		}
		else if (desc.isNonMoveable)
		{
			l_headerFile << "  INNO_CLASS_CONCRETE_NON_MOVABLE(" << desc.className << ");" << std::endl;
		}
		else if (desc.isNonCopyable)
		{
			l_headerFile << "  INNO_CLASS_CONCRETE_NON_COPYABLE(" << desc.className << ");" << std::endl;
		}
		else
		{
			l_headerFile << "  INNO_CLASS_CONCRETE_DEFALUT(" << desc.className << ");" << std::endl;
		}
	}

	l_headerFile << std::endl;
	l_headerFile << "  bool setup();" << std::endl;
	l_headerFile << "  bool initialize();" << std::endl;
	l_headerFile << "  bool update();" << std::endl;
	l_headerFile << "  bool terminate();" << std::endl;
	l_headerFile << "  ObjectStatus getStatus();" << std::endl;

	l_headerFile << std::endl;
	l_headerFile << "private:" << std::endl;
	l_headerFile << "  ObjectStatus m_objectStatus = ObjectStatus::Terminated;" << std::endl;
	l_headerFile << "};" << std::endl;

	l_headerFile.close();

	InnoLogger::Log(LogLevel::Success, "FileSystem: ", l_headerFileName.c_str(), " has been generated.");
	return true;
}