#if defined(MANUAL_SCRAPING)
#include "guis/GuiScraperStart.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiScraperMulti.h"
#include "views/ViewController.h"

#include "LocaleES.h"
#include "components/OptionListComponent.h"
#include "components/SwitchComponent.h"
#include "components/TextComponent.h"

#define BUTTON_BACK "a"
#define BUTTON_LAUNCH "b"

GuiScraperStart::GuiScraperStart(Window* window)
	: GuiComponent(window)
	, mMenu(window, _("SCRAPE NOW"))
	, mApproveResults(std::make_shared<SwitchComponent>(window))
{
	addChild(&mMenu);

	// add filters (with first one selected)
	mFilters = std::make_shared<OptionListComponent<GameFilterFunc>>(mWindow, _("SCRAPE THESE GAMES"), false);
	mFilters->add(_("All Games"), [](SystemData*, FileData*) { return true; }, false);
	mFilters->add(_("Only missing image"), [](SystemData*, FileData* fd) { return fd->metadata.get("image").empty(); }, true);
	mMenu.addWithLabel(_("FILTER"), mFilters);

	// add systems (all with a platform id specified selected)
	mSystems = std::make_shared<OptionListComponent<SystemData*>>(mWindow, _("SCRAPE THESE SYSTEMS"), true);
	for (const auto& it : SystemData::sSystemVector)
	{
		if (!it->hasPlatformId(PlatformIds::PLATFORM_IGNORE))
			mSystems->add(it->getFullName(), it, !it->getPlatformIds().empty());
	}
	mMenu.addWithLabel(_("SYSTEMS"), mSystems);

	mApproveResults->setState(true);
	mMenu.addWithLabel(_("USER DECIDES ON CONFLICTS"), mApproveResults);

	mMenu.addButton(_("START"), "start", std::bind(&GuiScraperStart::start, this));
	mMenu.addButton(_("BACK"), "back", [&] { delete this; });

	mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.1f);
}

void GuiScraperStart::start()
{
	std::vector<SystemData*> sys = mSystems->getSelectedObjects();
	for (const auto& it : sys)
	{
		if (it->getPlatformIds().empty())
		{
			mWindow->pushGui(new GuiMsgBox(mWindow,
				_("WARNING: SOME OF YOUR SELECTED SYSTEMS DO NOT HAVE A PLATFORM SET. "
				  "RESULTS MAY BE MORE INACCURATE THAN USUAL!\n"
				  "CONTINUE ANYWAY?"),
				_("YES"), std::bind(&GuiScraperStart::start, this), _("NO"), nullptr));
			return;
		}
	}

	const std::queue<ScraperSearchParams> searches = getSearches(mSystems->getSelectedObjects(), mFilters->getSelected());

	if (searches.empty())
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("NO GAMES FIT THAT CRITERIA.")));
	}
	else
	{
		mWindow->pushGui(new GuiScraperMulti(mWindow, searches, mApproveResults->getState()));
		delete this;
	}
}

std::queue<ScraperSearchParams> GuiScraperStart::getSearches(std::vector<SystemData*> systems, GameFilterFunc selector)
{
	std::queue<ScraperSearchParams> queue;
	for (const auto& sys : systems)
	{
		const std::vector<FileData*> games = sys->getRootFolder()->getFilesRecursive(GAME);
		for (const auto& game : games)
		{
			if (selector(sys, game))
				queue.push(ScraperSearchParams{ sys, game });
		}
	}

	return queue;
}

bool GuiScraperStart::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input)) // consumed?
		return true;

	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		delete this;
		return true;
	}

	if (config->isMappedTo("start", input) && input.value != 0)
	{
		// close everything
		Window* window = mWindow;
		while (window->peekGui() && window->peekGui() != ViewController::get()) // Why the loop here?
			delete window->peekGui();
	}

	return false;
}

std::vector<HelpPrompt> GuiScraperStart::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mMenu.getHelpPrompts();
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}
#endif