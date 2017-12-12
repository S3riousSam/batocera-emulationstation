#pragma once
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include <functional>

class GuiMenu : public GuiComponent
{
public:
	GuiMenu(Window* window);
#if defined(EXTENSION)
	~GuiMenu();
#endif

	bool input(InputConfig* config, Input input) override;
	void onSizeChanged() override;
	std::vector<HelpPrompt> getHelpPrompts() override;

	// private:
	void addEntry(const char* name, unsigned int color, bool add_arrow, const std::function<void()>& func);
	void addEntry(const std::string& name, unsigned int color, bool add_arrow, const std::function<void()>& func);

private:
	MenuComponent mMenu;
	TextComponent mVersion;
#if defined(EXTENSION)
public:
	struct StrInputConfig;
	std::vector<StrInputConfig*>
		mLoadedInput; // used to keep information about loaded devices in case there are unplugged between device window load and save
#endif
};
