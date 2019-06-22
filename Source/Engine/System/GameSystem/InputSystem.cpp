#include "InputSystem.h"

#include "../ICoreSystem.h"

extern ICoreSystem* g_pCoreSystem;

INNO_PRIVATE_SCOPE InnoInputSystemNS
{
	bool setup();
	bool initialize();
	bool update();
	bool terminate();

	bool addButtonStatusCallback(ButtonData boundButton, std::function<void()>* buttonStatusCallbackFunctor);
	bool addButtonStatusCallback(ButtonData boundButton, std::vector<std::function<void()>*>& buttonStatusCallbackFunctor);
	bool addButtonStatusCallback(ButtonStatusCallbackMap & buttonStatusCallbackFunctor);
	bool addMouseMovementCallback(int mouseCode, std::function<void(float)>* mouseMovementCallback);
	bool addMouseMovementCallback(int mouseCode, std::vector<std::function<void(float)>*>& mouseMovementCallback);
	bool addMouseMovementCallback(MouseMovementCallbackMap& mouseMovementCallback);

	vec4 getMousePositionInWorldSpace();
	vec2 getMousePositionInScreenSpace();

	void buttonStatusCallback(ButtonData boundButton);
	void framebufferSizeCallback(int width, int height);
	void mousePositionCallback(float mouseXPos, float mouseYPos);
	void scrollCallback(float xoffset, float yoffset);

	ObjectStatus m_objectStatus = ObjectStatus::Terminated;

	const InputConfig m_inputConfig = { 256, 5 };

	ButtonStatusMap m_buttonStatus;
	ButtonStatusCallbackMap m_buttonStatusCallback;
	MouseMovementCallbackMap m_mouseMovementCallback;

	float m_mouseXOffset;
	float m_mouseYOffset;
	float m_mouseLastX;
	float m_mouseLastY;

	vec4 m_mousePositionInWorldSpace;
};

bool InnoInputSystemNS::setup()
{
	InnoInputSystemNS::m_objectStatus = ObjectStatus::Created;
	return true;
}

bool InnoInputSystemNS::initialize()
{
	if (InnoInputSystemNS::m_objectStatus == ObjectStatus::Created)
	{
		for (int i = 0; i < m_inputConfig.totalKeyCodes; i++)
		{
			m_buttonStatus.emplace(i, ButtonStatus::RELEASED);
		}

		m_objectStatus = ObjectStatus::Activated;
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "InputSystem has been initialized.");

		return true;
	}
	else
	{
		g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_ERROR, "InputSystem: Object is not created!");
		return false;
	}
}

bool InnoInputSystemNS::update()
{
	if (InnoInputSystemNS::m_objectStatus == ObjectStatus::Activated)
	{
		m_buttonStatus = g_pCoreSystem->getWindowSystem()->getButtonStatus();

		for (auto& i : m_buttonStatus)
		{
			auto l_keybinding = m_buttonStatusCallback.find(ButtonData{ i.first, i.second });
			if (l_keybinding != m_buttonStatusCallback.end())
			{
				for (auto& j : l_keybinding->second)
				{
					if (j)
					{
						(*j)();
					}
				}
			}
		}

		if (m_mouseMovementCallback.size() != 0)
		{
			if (m_mouseXOffset != 0)
			{
				for (auto& j : m_mouseMovementCallback.find(0)->second)
				{
					(*j)(m_mouseXOffset);
				};
			}
			if (m_mouseYOffset != 0)
			{
				for (auto& j : m_mouseMovementCallback.find(1)->second)
				{
					(*j)(m_mouseYOffset);
				};
			}
			if (m_mouseXOffset != 0 || m_mouseYOffset != 0)
			{
				m_mouseXOffset = 0;
				m_mouseYOffset = 0;
			}
		}

		return true;
	}
	else
	{
		InnoInputSystemNS::m_objectStatus = ObjectStatus::Suspended;
		return false;
	}

	return true;
}

bool InnoInputSystemNS::terminate()
{
	m_objectStatus = ObjectStatus::Terminated;
	g_pCoreSystem->getLogSystem()->printLog(LogType::INNO_DEV_SUCCESS, "InputSystem has been terminated.");

	return true;
}

bool InnoInputSystemNS::addButtonStatusCallback(ButtonData boundButton, std::function<void()>* buttonStatusCallbackFunctor)
{
	auto l_keyboardInputCallbackFunctionVector = m_buttonStatusCallback.find(boundButton);
	if (l_keyboardInputCallbackFunctionVector != m_buttonStatusCallback.end())
	{
		l_keyboardInputCallbackFunctionVector->second.emplace_back(buttonStatusCallbackFunctor);
	}
	else
	{
		m_buttonStatusCallback.emplace(boundButton, std::vector<std::function<void()>*>{buttonStatusCallbackFunctor});
	}

	return true;
}

bool InnoInputSystemNS::addButtonStatusCallback(ButtonData boundButton, std::vector<std::function<void()>*>& buttonStatusCallbackFunctor)
{
	for (auto& i : buttonStatusCallbackFunctor)
	{
		addButtonStatusCallback(boundButton, i);
	}

	return true;
}

bool InnoInputSystemNS::addButtonStatusCallback(ButtonStatusCallbackMap & buttonStatusCallbackFunctor)
{
	for (auto& i : buttonStatusCallbackFunctor)
	{
		addButtonStatusCallback(i.first, i.second);
	}

	return true;
}

bool InnoInputSystemNS::addMouseMovementCallback(int mouseCode, std::function<void(float)>* mouseMovementCallback)
{
	auto l_mouseMovementCallbackFunctionVector = m_mouseMovementCallback.find(mouseCode);
	if (l_mouseMovementCallbackFunctionVector != m_mouseMovementCallback.end())
	{
		l_mouseMovementCallbackFunctionVector->second.emplace_back(mouseMovementCallback);
	}
	else
	{
		m_mouseMovementCallback.emplace(mouseCode, std::vector<std::function<void(float)>*>{mouseMovementCallback});
	}

	return true;
}

bool InnoInputSystemNS::addMouseMovementCallback(int mouseCode, std::vector<std::function<void(float)>*>& mouseMovementCallback)
{
	for (auto& i : mouseMovementCallback)
	{
		addMouseMovementCallback(mouseCode, i);
	}

	return true;
}

bool InnoInputSystemNS::addMouseMovementCallback(MouseMovementCallbackMap & mouseMovementCallback)
{
	for (auto& i : mouseMovementCallback)
	{
		addMouseMovementCallback(i.first, i.second);
	}

	return true;
}

vec4 InnoInputSystemNS::getMousePositionInWorldSpace()
{
	auto l_screenResolution = g_pCoreSystem->getRenderingFrontend()->getScreenResolution();

	auto l_x = 2.0f * m_mouseLastX / l_screenResolution.x - 1.0f;
	auto l_y = 1.0f - 2.0f * m_mouseLastY / l_screenResolution.y;
	auto l_z = -1.0f;
	auto l_w = 1.0f;
	vec4 l_ndcSpace = vec4(l_x, l_y, l_z, l_w);

	auto l_cameraComponents = g_pCoreSystem->getGameSystem()->get<CameraComponent>();
	auto l_mainCamera = l_cameraComponents[0];
	auto pCamera = l_mainCamera->m_projectionMatrix;
	auto l_cameraTransformComponent = g_pCoreSystem->getGameSystem()->get<TransformComponent>(l_mainCamera->m_parentEntity);
	auto rCamera =
		InnoMath::getInvertRotationMatrix(
			l_cameraTransformComponent->m_globalTransformVector.m_rot
		);
	auto tCamera =
		InnoMath::getInvertTranslationMatrix(
			l_cameraTransformComponent->m_globalTransformVector.m_pos
		);
	//Column-Major memory layout
#ifdef USE_COLUMN_MAJOR_MEMORY_LAYOUT
	l_ndcSpace = InnoMath::mul(l_ndcSpace, pCamera.inverse());
	l_ndcSpace.z = -1.0f;
	l_ndcSpace.w = 0.0f;
	l_ndcSpace = InnoMath::mul(l_ndcSpace, rCamera.inverse());
	l_ndcSpace = InnoMath::mul(l_ndcSpace, tCamera.inverse());
#endif
	//Row-Major memory layout
#ifdef USE_ROW_MAJOR_MEMORY_LAYOUT

	l_ndcSpace = InnoMath::mul(pCamera.inverse(), l_ndcSpace);
	l_ndcSpace.z = -1.0f;
	l_ndcSpace.w = 0.0f;
	l_ndcSpace = InnoMath::mul(tCamera.inverse(), l_ndcSpace);
	l_ndcSpace = InnoMath::mul(rCamera.inverse(), l_ndcSpace);
#endif
	l_ndcSpace = l_ndcSpace.normalize();
	return l_ndcSpace;
}

vec2 InnoInputSystemNS::getMousePositionInScreenSpace()
{
	return vec2(m_mouseLastX, m_mouseLastY);
}

void InnoInputSystemNS::buttonStatusCallback(ButtonData boundButton)
{
	auto l_keybinding = m_buttonStatusCallback.find(boundButton);
	if (l_keybinding != m_buttonStatusCallback.end())
	{
		for (auto& j : l_keybinding->second)
		{
			if (j)
			{
				(*j)();
			}
		}
	}
}

void InnoInputSystemNS::framebufferSizeCallback(int width, int height)
{
	TVec2<unsigned int> l_newScreenResolution = TVec2<unsigned int>(width, height);
	g_pCoreSystem->getRenderingFrontend()->setScreenResolution(l_newScreenResolution);
	g_pCoreSystem->getRenderingBackend()->resize();
}

void InnoInputSystemNS::mousePositionCallback(float mouseXPos, float mouseYPos)
{
	m_mouseXOffset = mouseXPos - m_mouseLastX;
	m_mouseYOffset = m_mouseLastY - mouseYPos;

	m_mouseLastX = mouseXPos;
	m_mouseLastY = mouseYPos;
}

void InnoInputSystemNS::scrollCallback(float xoffset, float yoffset)
{
}

bool InnoInputSystem::setup()
{
	return InnoInputSystemNS::setup();
}

bool InnoInputSystem::initialize()
{
	return InnoInputSystemNS::initialize();
}

bool InnoInputSystem::update()
{
	return InnoInputSystemNS::update();
}

bool InnoInputSystem::terminate()
{
	return InnoInputSystemNS::terminate();
}

InputConfig InnoInputSystem::getInputConfig()
{
	return InnoInputSystemNS::m_inputConfig;
}

void InnoInputSystem::addButtonStatusCallback(ButtonData boundButton, std::function<void()>* buttonStatusCallbackFunctor)
{
	InnoInputSystemNS::addButtonStatusCallback(boundButton, buttonStatusCallbackFunctor);
}

void InnoInputSystem::addButtonStatusCallback(ButtonData boundButton, std::vector<std::function<void()>*>& buttonStatusCallbackFunctor)
{
	InnoInputSystemNS::addButtonStatusCallback(boundButton, buttonStatusCallbackFunctor);
}

void InnoInputSystem::addButtonStatusCallback(ButtonStatusCallbackMap & buttonStatusCallbackFunctor)
{
	InnoInputSystemNS::addButtonStatusCallback(buttonStatusCallbackFunctor);
}

void InnoInputSystem::addMouseMovementCallback(int mouseCode, std::function<void(float)>* mouseMovementCallback)
{
	InnoInputSystemNS::addMouseMovementCallback(mouseCode, mouseMovementCallback);
}

void InnoInputSystem::addMouseMovementCallback(int mouseCode, std::vector<std::function<void(float)>*>& mouseMovementCallback)
{
	InnoInputSystemNS::addMouseMovementCallback(mouseCode, mouseMovementCallback);
}

void InnoInputSystem::addMouseMovementCallback(MouseMovementCallbackMap& mouseMovementCallback)
{
	InnoInputSystemNS::addMouseMovementCallback(mouseMovementCallback);
}

void InnoInputSystem::buttonStatusCallback(ButtonData boundButton)
{
	InnoInputSystemNS::buttonStatusCallback(boundButton);
}

void InnoInputSystem::framebufferSizeCallback(int width, int height)
{
	InnoInputSystemNS::framebufferSizeCallback(width, height);
}

void InnoInputSystem::mousePositionCallback(float mouseXPos, float mouseYPos)
{
	InnoInputSystemNS::mousePositionCallback(mouseXPos, mouseYPos);
}

void InnoInputSystem::scrollCallback(float xoffset, float yoffset)
{
}

vec4 InnoInputSystem::getMousePositionInWorldSpace()
{
	return InnoInputSystemNS::getMousePositionInWorldSpace();
}

vec2 InnoInputSystem::getMousePositionInScreenSpace()
{
	return InnoInputSystemNS::getMousePositionInScreenSpace();
}

ObjectStatus InnoInputSystem::getStatus()
{
	return InnoInputSystemNS::m_objectStatus;
}