#include "guis/GuiSettings.h"
#include "LocaleES.h"
#include "Settings.h"
#include "Window.h"
#include "views/ViewController.h"

GuiSettings::GuiSettings(Window* window, const char* title)
	: GuiComponent(window)
	, mMenu(window, title)
{
	addChild(&mMenu);

	mMenu.addButton(_("BACK"), "go back", [this] { delete this; });

	setSize(Renderer::getScreenSize());
	mMenu.setPosition((mSize.x() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.1f);
}

GuiSettings::GuiSettings(Window* window, const std::string& title)
	: GuiSettings(window, title.c_str())
{
}

GuiSettings::~GuiSettings()
{
#if defined(EXTENSION)
	if (doSave)
#endif
	{
		save();
	}
}

void GuiSettings::save()
{
	if (!mSaveFuncs.size())
		return;

	for (auto it = mSaveFuncs.begin(); it != mSaveFuncs.end(); it++)
		(*it)();

	Settings::getInstance()->saveFile();
}

bool GuiSettings::input(InputConfig* config, Input input)
{
	if (config->isMappedTo(BUTTON_BACK, input) && input.value != 0)
	{
		delete this;
		return true;
	}

	if (config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		while (mWindow->peekGui() != nullptr && mWindow->peekGui() != ViewController::get())
			delete mWindow->peekGui();
		return true;
	}

	return GuiComponent::input(config, input);
}

std::vector<HelpPrompt> GuiSettings::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();

	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));

	return prompts;
}
