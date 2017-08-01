#pragma once
#include "IEventManager.h"
#include "WindowManager.h"

class GUIManager : public IEventManager
{
public:
	~GUIManager();

	static GUIManager& getInstance()
	{
		static GUIManager instance;
		return instance;
	}

	struct nk_context* ctx;
	struct nk_color* background;
	struct nk_font_atlas* atlas;

	glm::vec3 getColor() const;

private:
	GUIManager();

	enum { EASY, HARD };
	 int op = EASY;
	 int property = 20;
	 int r = 255;
	 int g = 255;
	 int b = 255;

	void init() override;
	void update() override;
	void shutdown() override;
};
