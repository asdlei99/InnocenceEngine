#pragma once
#include "BaseComponent.h"
#include <iostream>
#include "../common/InnoConcurrency.h"
class LogSystemSingletonComponent : public BaseComponent
{
public:
	~LogSystemSingletonComponent() {};
	
	static LogSystemSingletonComponent& getInstance()
	{
		static LogSystemSingletonComponent instance;
		//std::cout << &instance << std::endl;
		return instance;
	}

ThreadSafeQueue<std::string> m_log; 

private:
	LogSystemSingletonComponent() {};
};
