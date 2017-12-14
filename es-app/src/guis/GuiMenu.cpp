#include "guis/GuiMenu.h"
#include "components/ButtonComponent.h"
#include "components/MenuComponent.h"
#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/SwitchComponent.h"
#include "components/TextComponent.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiMsgBox.h"
#if defined(MANUAL_SCRAPING)
#include "guis/GuiScraperStart.h"
#endif
#include "guis/GuiSettings.h"
#include "scrapers/GamesDBScraper.h"
#include "views/ViewController.h"
//#include "scrapers/TheArchiveScraper.h"
#include "EmulationStation.h"
#include "Log.h"
#include "Settings.h"
#include "Sound.h"
#include "VolumeControl.h"
#include "Window.h"

#if defined(EXTENSION)
#include "AudioManager.h"
#include "RecalboxConf.h"
#include "SystemInterface.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
namespace GuiMenuEx
{
	void createInputTextRow(Window* window, GuiSettings* gui, std::string title, const char* settingsID, bool password);
	void AddMenuItems(GuiMenu& menu, Window* window);
	void AddSoundSettings(GuiSettings& gui, Window* window, std::shared_ptr<SliderComponent>& volume);
	void AddSettingsUI(GuiSettings& gui, Window* window);
	void AddMenuNetwork(GuiMenu& menu, Window* window);
	std::shared_ptr<OptionListComponent<std::string>> createRatioOptionList(Window* window, std::string configname);
	void clearLoadedInput(std::vector<GuiMenu::StrInputConfig*>& v);
	void AddAutoScrape(GuiSettings& gui, Window* window);
	void AddMenuScrape(GuiSettings& gui, Window* window, std::function<void()> handler);
} // namespace GuiMenuEx
#else
#include "LocaleES.h"
#endif

#define BUTTON_BACK "a"
#define BUTTON_LAUNCH "b"

GuiMenu::GuiMenu(Window* window)
	: GuiComponent(window)
	, mMenu(window, _("MAIN MENU"))
	, mVersion(window)
{
	// MAIN MENU

	// KODI >
	// ROM MANAGER >
	// SYSTEM >
	// GAMES >
	// CONTROLLERS >
	// UI SETTINGS >
	// SOUND SETTINGS >
	// NETWORK >
	// SCRAPER >
	// QUIT >
#if defined(MANUAL_SCRAPING)
#if defined(EXTENSION)
	std::function<void()> manualScrape = [this]
#else // !defined(EXTENSION)
	auto openScrapeNow = [this] { mWindow->pushGui(new GuiScraperStart(mWindow)); };
	addEntry(_("SCRAPER"), 0x777777FF, true,
		[this, openScrapeNow]
#endif
	{
#if defined(EXTENSION)
		auto openScrapeNow = [this] { mWindow->pushGui(new GuiScraperStart(mWindow)); };
		auto s = new GuiSettings(mWindow, _("MANUAL SCRAPER"));
#else
		auto s = new GuiSettings(mWindow, _("SCRAPER"));
#endif
		// scrape from
		auto scraperList = std::make_shared<OptionListComponent<std::string>>(mWindow, _("SCRAPE FROM"), false);
		for (const auto& it : Scraper::getScraperList())
			scraperList->add(it, it, (it == Settings::getInstance()->getString("Scraper")));

		s->addWithLabel(_("SCRAPE FROM"), scraperList);
		s->addSaveFunc([scraperList] { Settings::getInstance()->setString("Scraper", scraperList->getSelected()); });

		// scrape ratings
		auto scrapeRatings = std::make_shared<SwitchComponent>(mWindow);
		scrapeRatings->setState(Settings::getInstance()->getBool("ScrapeRatings"));
		s->addWithLabel(_("SCRAPE RATINGS"), scrapeRatings);
		s->addSaveFunc([scrapeRatings] { Settings::getInstance()->setBool("ScrapeRatings", scrapeRatings->getState()); });

		// scrape now
		ComponentListRow row;
		row.makeAcceptInputHandler([s, openScrapeNow] {
			s->save();
			openScrapeNow();
		});

		auto scrape_now = std::make_shared<TextComponent>(mWindow, _("SCRAPE NOW"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
		auto bracket = makeArrow(mWindow);
		row.addElement(scrape_now, true);
		row.addElement(bracket, false);
		s->addRow(row);

		mWindow->pushGui(s);
#if defined(EXTENSION)
	};
#else
	});
#endif
#endif // MANUAL_SCRAPING

#if defined(EXTENSION)
	std::function<void()> soundsettings = [this]
#else
	addEntry(_("SOUND SETTINGS"), 0x777777FF, true,
		[this]
#endif
	{
		auto s = new GuiSettings(mWindow, _("SOUND SETTINGS"));
		// volume
		auto volume = std::make_shared<SliderComponent>(mWindow, 0.f, 100.f, 1.f, "%");
		volume->setValue((float)VolumeControl::getInstance()->getVolume());
		s->addWithLabel(_("SYSTEM VOLUME"), volume);
		s->addSaveFunc([volume] { VolumeControl::getInstance()->setVolume((int)round(volume->getValue())); });
#if defined(EXTENSION)
		GuiMenuEx::AddSoundSettings(*s, mWindow, volume);
#else
			// disable sounds
			auto sounds_enabled = std::make_shared<SwitchComponent>(mWindow);
			sounds_enabled->setState(Settings::getInstance()->getBool("EnableSounds"));
			s->addWithLabel(_("ENABLE SOUNDS"), sounds_enabled);
			s->addSaveFunc([sounds_enabled] { Settings::getInstance()->setBool("EnableSounds", sounds_enabled->getState()); });
#endif
		mWindow->pushGui(s);
#if defined(EXTENSION)
	};
#else
		});
#endif

#if defined(EXTENSION)
	GuiMenuEx::AddMenuItems(*this, mWindow);
#endif
#if defined(EXTENSION)
	if (RecalboxConf::get("system.es.menu") != "bartop")
#endif
		addEntry(_("UI SETTINGS"), 0x777777FF, true, [this] {
			auto s = new GuiSettings(mWindow, _("UI SETTINGS"));

#if defined(EXTENSION)
			GuiMenuEx::AddSettingsUI(*s, mWindow);
#endif
			// screensaver time
			auto screensaver_time = std::make_shared<SliderComponent>(mWindow, 0.f, 30.f, 1.f, "m");
			screensaver_time->setValue((float)(Settings::getInstance()->getInt("ScreenSaverTime") / (1000 * 60)));
			s->addWithLabel(_("SCREENSAVER AFTER"), screensaver_time);
			s->addSaveFunc(
				[screensaver_time] { Settings::getInstance()->setInt("ScreenSaverTime", (int)round(screensaver_time->getValue()) * (1000 * 60)); });

			// screensaver behavior
			auto screensaver_behavior = std::make_shared<OptionListComponent<std::string>>(mWindow, _("TRANSITION STYLE"), false);
			std::vector<std::string> screensavers;
			screensavers.push_back("dim");
			screensavers.push_back("black");
			for (auto it = screensavers.begin(); it != screensavers.end(); it++)
				screensaver_behavior->add(*it, *it, Settings::getInstance()->getString("ScreenSaverBehavior") == *it);
			s->addWithLabel(_("SCREENSAVER BEHAVIOR"), screensaver_behavior);
			s->addSaveFunc(
				[screensaver_behavior] { Settings::getInstance()->setString("ScreenSaverBehavior", screensaver_behavior->getSelected()); });

			// framerate
			auto framerate = std::make_shared<SwitchComponent>(mWindow);
			framerate->setState(Settings::getInstance()->getBool("DrawFramerate"));
			s->addWithLabel(_("SHOW FRAMERATE"), framerate);
			s->addSaveFunc([framerate] { Settings::getInstance()->setBool("DrawFramerate", framerate->getState()); });

			// show help
			auto show_help = std::make_shared<SwitchComponent>(mWindow);
			show_help->setState(Settings::getInstance()->getBool("ShowHelpPrompts"));
			s->addWithLabel(_("ON-SCREEN HELP"), show_help);
			s->addSaveFunc([show_help] { Settings::getInstance()->setBool("ShowHelpPrompts", show_help->getState()); });

			// quick system select (left/right in game list view)
			auto quick_sys_select = std::make_shared<SwitchComponent>(mWindow);
			quick_sys_select->setState(Settings::getInstance()->getBool("QuickSystemSelect"));
			s->addWithLabel(_("QUICK SYSTEM SELECT"), quick_sys_select);
			s->addSaveFunc([quick_sys_select] { Settings::getInstance()->setBool("QuickSystemSelect", quick_sys_select->getState()); });

#if defined(EXTENSION)
			// Enable OSK (On-Screen-Keyboard)
			auto osk_enable = std::make_shared<SwitchComponent>(mWindow);
			osk_enable->setState(Settings::getInstance()->getBool("UseOSK"));
			s->addWithLabel(_("ON SCREEN KEYBOARD"), osk_enable);
			s->addSaveFunc([osk_enable] { Settings::getInstance()->setBool("UseOSK", osk_enable->getState()); });
#endif
			// transition style
			auto transition_style = std::make_shared<OptionListComponent<std::string>>(mWindow, _("TRANSITION STYLE"), false);
			std::vector<std::string> transitions;
			transitions.push_back("fade");
			transitions.push_back("slide");
			for (auto it = transitions.begin(); it != transitions.end(); it++)
				transition_style->add(*it, *it, Settings::getInstance()->getString("TransitionStyle") == *it);
			s->addWithLabel(_("TRANSITION STYLE"), transition_style);
			s->addSaveFunc([transition_style] { Settings::getInstance()->setString("TransitionStyle", transition_style->getSelected()); });

			// theme set
			auto themeSets = ThemeData::getThemeSets();

			if (!themeSets.empty())
			{
				auto selectedSet = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
				if (selectedSet == themeSets.end())
					selectedSet = themeSets.begin();

				auto theme_set = std::make_shared<OptionListComponent<std::string>>(mWindow, _("THEME SET"), false);
				for (auto it = themeSets.begin(); it != themeSets.end(); it++)
					theme_set->add(it->first, it->first, it == selectedSet);
				s->addWithLabel(_("THEME SET"), theme_set);

				Window* window = mWindow;
				s->addSaveFunc([window, theme_set] {
					bool needReload = false;
					if (Settings::getInstance()->getString("ThemeSet") != theme_set->getSelected())
						needReload = true;

					Settings::getInstance()->setString("ThemeSet", theme_set->getSelected());

					if (needReload)
						ViewController::get()->reloadAll(); // TODO - replace this with some sort of signal-based implementation
				});
			}

			mWindow->pushGui(s);
		});

#if !defined(EXTENSION)
	addEntry("INPUT SETTINGS", 0x777777FF, true, [this] { mWindow->pushGui(new GuiDetectDevice(mWindow, false, nullptr)); });
#endif
#if defined(EXTENSION)
	addEntry(_("SOUND SETTINGS"), 0x777777FF, true, soundsettings);

	GuiMenuEx::AddMenuNetwork(*this, mWindow);

	if (RecalboxConf::get("system.es.menu") != "bartop")
	{
#if defined(MANUAL_SCRAPING)
		// manual or automatic?
		addEntry(_("SCRAPER"), 0x777777FF, true, [this, manualScrape] {
			auto s = new GuiSettings(mWindow, _("SCRAPER"));
			GuiMenuEx::AddAutoScrape(*s, mWindow);
			GuiMenuEx::AddMenuScrape(*s, mWindow, manualScrape);
			mWindow->pushGui(s);
		});
#else
		// manual or automatic? ...automatic!
		addEntry(_("SCRAPER"), 0x777777FF, true, [this] {
			auto s = new GuiSettings(mWindow, _("SCRAPER"));
			GuiMenuEx::AddAutoScrape(*s, mWindow);
			mWindow->pushGui(s);
		});
#endif
	}
#endif

	addEntry(_("QUIT"), 0x777777FF, true, [this] {
		auto s = new GuiSettings(mWindow, _("QUIT"));
#if defined(EXTENSION) // TODO: Duplicate Code!
		Window* window = mWindow;

		ComponentListRow row;
		row.makeAcceptInputHandler([window] {
			window->pushGui(new GuiMsgBox(window, _("REALLY RESTART?"), _("YES"),
				[] {
					if (SystemInterface::reboot() != 0)
						LOG(LogWarning) << "Restart terminated with non-zero result!";
				},
				_("NO"), nullptr));
		});
		row.addElement(std::make_shared<TextComponent>(window, _("RESTART SYSTEM"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
		s->addRow(row);

		row.elements.clear();
		row.makeAcceptInputHandler([window] {
			window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN?"), _("YES"),
				[] {
					if (SystemInterface::shutdown() != 0)
						LOG(LogWarning) << "Shutdown terminated with non-zero result!";
				},
				_("NO"), nullptr));
		});
		row.addElement(std::make_shared<TextComponent>(window, _("SHUTDOWN SYSTEM"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
		s->addRow(row);

#if defined(EXTENSION)
		row.elements.clear();
		row.makeAcceptInputHandler([window] {
			window->pushGui(new GuiMsgBox(window, _("REALLY SHUTDOWN WITHOUT SAVING METADATAS?"), _("YES"),
				[] {
					if (SystemInterface::fastShutdown() != 0)
					{
						LOG(LogWarning) << "Shutdown terminated with non-zero result!";
					}
				},
				_("NO"), nullptr));
		});
		row.addElement(std::make_shared<TextComponent>(window, _("FAST SHUTDOWN SYSTEM"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
		s->addRow(row);
#else
		if(Settings::getInstance()->getBool("ShowExit"))
		{
			row.elements.clear();
			row.makeAcceptInputHandler([window] {
				window->pushGui(new GuiMsgBox(window, _("REALLY QUIT?"), _("YES"),
				[] {
					SDL_Event ev;
					ev.type = SDL_QUIT;
					SDL_PushEvent(&ev);
				}, _("NO"), nullptr));
			});
			row.addElement(std::make_shared<TextComponent>(window, _("QUIT EMULATIONSTATION"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF), true);
			s->addRow(row);
		}
#endif
		// ViewController::get()->reloadAll();
#endif
		mWindow->pushGui(s);
	});

	mVersion.setFont(Font::get(FONT_SIZE_SMALL));
	mVersion.setColor(0xC6C6C6FF);
	mVersion.setText("EMULATIONSTATION V" + strToUpper(PROGRAM_VERSION_STRING));
	mVersion.setAlignment(ALIGN_CENTER);

	addChild(&mMenu);
	addChild(&mVersion);

	setSize(mMenu.getSize());
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, Renderer::getScreenHeight() * 0.1f);
}

#if defined(EXTENSION)
GuiMenu::~GuiMenu()
{
	GuiMenuEx::clearLoadedInput(mLoadedInput);
}
#endif

void GuiMenu::onSizeChanged()
{
	mVersion.setSize(mSize.x(), 0);
	mVersion.setPosition(0, mSize.y() - mVersion.getSize().y());
}

void GuiMenu::addEntry(const char* name, unsigned int color, bool add_arrow, const std::function<void()>& func)
{
	std::shared_ptr<Font> font = Font::get(FONT_SIZE_MEDIUM);

	// populate the list
	ComponentListRow row;
	row.addElement(std::make_shared<TextComponent>(mWindow, name, font, color), true);

	if (add_arrow)
	{
		std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);
		row.addElement(bracket, false);
	}

	row.makeAcceptInputHandler(func);

	mMenu.addRow(row);
}

void GuiMenu::addEntry(const std::string& name, unsigned int color, bool add_arrow, const std::function<void()>& func)
{
	// return addEntry(name, color, add_arrow, func); // 'GuiMenu::addEntry': recursive on all control paths, function will cause runtime stack
	// overflow

	std::shared_ptr<Font> font = Font::get(FONT_SIZE_MEDIUM);

	// populate the list
	ComponentListRow row;
	row.addElement(std::make_shared<TextComponent>(mWindow, name.c_str(), font, color), true);

	if (add_arrow)
	{
		std::shared_ptr<ImageComponent> bracket = makeArrow(mWindow);
		row.addElement(bracket, false);
	}

	row.makeAcceptInputHandler(func);

	mMenu.addRow(row);
}

bool GuiMenu::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	if ((config->isMappedTo(BUTTON_BACK, input) || config->isMappedTo("start", input)) && input.value != 0)
	{
		delete this;
		return true;
	}

	return false;
}

std::vector<HelpPrompt> GuiMenu::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("up/down", _("CHOOSE")));
	prompts.push_back(HelpPrompt(BUTTON_LAUNCH, _("SELECT")));
	prompts.push_back(HelpPrompt("start", _("CLOSE")));
	return prompts;
}
