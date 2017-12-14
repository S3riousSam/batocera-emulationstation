#if defined(MANUAL_SCRAPING)
#pragma once
#include "GuiComponent.h"
#include "SystemData.h"
#include "components/MenuComponent.h"
#include "scrapers/Scraper.h"
#include <queue>

template<typename T>
class OptionListComponent;

class SwitchComponent;

// Performs multi-game scraping (starting point) allowing the user to set various parameters
// (filters, target systems, enabling manual mode).
class GuiScraperStart : public GuiComponent
{
public:
	GuiScraperStart(Window* window);

	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	typedef std::function<bool(SystemData*, FileData*)> GameFilterFunc;

	void pressedStart();
	void start();
	std::queue<ScraperSearchParams> getSearches(std::vector<SystemData*> systems, GameFilterFunc selector);

	std::shared_ptr<OptionListComponent<GameFilterFunc>> mFilters;
	std::shared_ptr<OptionListComponent<SystemData*>> mSystems;
	std::shared_ptr<SwitchComponent> mApproveResults;

	MenuComponent mMenu;
};
#endif