#pragma once
#include "../Interface/ITestSystem.h"

class InnoTestSystem : public ITestSystem
{
public:
	INNO_CLASS_CONCRETE_NON_COPYABLE(InnoTestSystem);

	bool setup() override;
	bool initialize() override;
	bool update() override;
	bool terminate() override;

	ObjectStatus getStatus() override;

	bool measure(const std::string& functorName, const std::function<void()>& functor) override;
};
