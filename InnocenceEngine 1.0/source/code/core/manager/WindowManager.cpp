#include "../../main/stdafx.h"
#include "WindowManager.h"


WindowManager::WindowManager()
{
}


WindowManager::~WindowManager()
{
}

GLFWwindow * WindowManager::getWindow() const
{
	return m_window;
}

glm::vec2 WindowManager::getScreenCenterPosition() const
{
	return glm::vec2(SCR_WIDTH / 2.0f, SCR_HEIGHT / 2.0f);
}

void WindowManager::setWindowName(const std::string & windowName)
{
	m_windowName = windowName;
}

void WindowManager::init()
{
	if (glfwInit() != GL_TRUE)
	{
		this->setStatus(objectStatus::ERROR);
		LogManager::getInstance().printLog("Failed to initialize GLFW.");
	}

	glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); //We don't want the old OpenGL 

	// Open a window and create its OpenGL context
	m_window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, m_windowName.c_str(), NULL, NULL);
	if (m_window == nullptr) {
		this->setStatus(objectStatus::ERROR);
		LogManager::getInstance().printLog("Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.");
		glfwTerminate();
	}
	glfwMakeContextCurrent(m_window); // Initialize GLEW
	glewExperimental = true; // Needed in core profile
	if (glewInit() != GLEW_OK) {
		this->setStatus(objectStatus::ERROR);
		LogManager::getInstance().printLog("Failed to initialize GLEW.");
	}
	this->setStatus(objectStatus::INITIALIZIED);
	LogManager::getInstance().printLog("WindowManager has been initialized.");
}

void WindowManager::update()
{
	if (m_window != nullptr && glfwWindowShouldClose(m_window) == 0) {
		glfwPollEvents();
		glfwSwapBuffers(m_window);
	}
	else
	{
		this->setStatus(objectStatus::STANDBY);
		LogManager::getInstance().printLog("WindowManager is stand-by.");
	}
}

void WindowManager::shutdown()
{
	if (m_window != nullptr)
	{
		glfwDestroyWindow(m_window);
		glfwTerminate();
		this->setStatus(objectStatus::UNINITIALIZIED);
		LogManager::getInstance().printLog("WindowManager has been shutdown.");
	}
}