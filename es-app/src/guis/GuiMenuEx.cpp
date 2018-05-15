#if defined(EXTENSION)
#include "AudioManager.h"
#include "GuiBackupStart.h"
#include "GuiMenu.h"
#include "components/OptionListComponent.h"
#include "components/SliderComponent.h"
#include "components/ComponentList.h"
#include "components/SwitchComponent.h"
#include "guis/GuiAutoScrape.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiInstallStart.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditPopup.h"
#include "guis/GuiTextEditPopupKeyboard.h"
#if defined(OBSOLETE)
#include "guis/GuiRomsManager.h"
#endif
#include "GuiLoading.h"
#include "GuiSettings.h"
#include "GuiUpdate.h"
#include "InputManager.h"
#include "LibretroRatio.h"
#include "LocaleES.h"
#include "RecalboxConf.h"
#include "Settings.h"
#include "SystemData.h"
#include "SystemInterface.h"
#include "VolumeControl.h"
#include "Window.h"
#include "views/ViewController.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>

#define MENU_SYSTEM_SETTINGS_ENABLED
#define MENU_GAMES_SETTINGS_ENABLED
#define MENU_CONTROLLERS_SETTINGS_ENABLED
#define MENU_NETWORK_SETTINGS_ENABLED

struct GuiMenu::StrInputConfig
{
	StrInputConfig(const std::string& ideviceName, const std::string& ideviceGUIDString)
		: deviceName(ideviceName)
		, deviceGUIDString(ideviceGUIDString)
	{
	}

	std::string deviceName;
	std::string deviceGUIDString;
};

namespace
{
	bool IsBartopOptionEnabled()
	{
		return (RecalboxConf::get("system.es.menu") == "bartop");
	}
} // namespace

namespace GuiMenuEx
{
	void createInputTextRow(Window* window, GuiSettings* gui, std::string title, const char* settingsID, bool password);
	void AddMenuItems(GuiMenu& menu, Window* window);

	void AddSoundSettings(GuiSettings& gui, Window* window, std::shared_ptr<SliderComponent>& volume)
	{
		auto sounds_enabled = std::make_shared<SwitchComponent>(window, !(RecalboxConf::get("audio.bgmusic") == "0"));
		gui.addWithLabel(_("FRONTEND MUSIC"), sounds_enabled);

		// audio device
		auto optionsAudio = std::make_shared<OptionListComponent<std::string>>(window, _("OUTPUT DEVICE"), false);
		const std::string currentDevice = RecalboxConf::get("audio.device", "auto");
		const std::vector<std::string> availableAudio = SystemInterface::getAvailableAudioOutputDevices();
		const std::string selectedAudio = SystemInterface::getCurrentAudioOutputDevice();

		if (!IsBartopOptionEnabled())
		{
			for (const auto& it : availableAudio)
			{
				std::vector<std::string> tokens;
				boost::split(tokens, it, boost::is_any_of(" "));
				if (tokens.size() >= 2)
				{
					// concatenate the ending words
					std::string vname = "";
					for (unsigned int i = 1; i < tokens.size(); i++)
					{
						if (i > 2)
							vname += " ";
						vname += tokens.at(i);
					}
					optionsAudio->add(vname, it, selectedAudio == it);
				}
				else
				{
					optionsAudio->add(it, it, selectedAudio == it);
				}
			}
			gui.addWithLabel(_("OUTPUT DEVICE"), optionsAudio);
		}

		gui.addSaveFunc([optionsAudio, currentDevice, sounds_enabled, &volume, window] {
			bool v_need_reboot = false;

			VolumeControl::getInstance()->setVolume(static_cast<int>(round(volume->getValue())));
			RecalboxConf::set("audio.volume", std::to_string(static_cast<int>(round(volume->getValue()))));
			RecalboxConf::set("audio.bgmusic", sounds_enabled->getState() ? "1" : "0");
			if (!sounds_enabled->getState())
				AudioManager::getInstance()->stopMusic();
			else
				AudioManager::getInstance()->playRandomMusic();

			if (currentDevice != optionsAudio->getSelected())
			{
				RecalboxConf::set("audio.device", optionsAudio->getSelected());
				SystemInterface::setAudioOutputDevice(optionsAudio->getSelected());
				v_need_reboot = true;
			}
			RecalboxConf::saveRecalboxConf();
			if (v_need_reboot)
			{
				// "The system must reboot to apply the change"
				window->pushGui(new GuiMsgBox(window, _("YOU NEED TO REBOOT THE SYSTEM TO COMPLETLY APPLY THIS OPTION."), _("OK"), [] {}));
			}
		});
	}

	void AddMenuNetwork(GuiMenu& menu, Window* window);

	void AddSettingsUI(GuiSettings& gui, Window* window)
	{
		// video device
		auto optionsVideo = std::make_shared<OptionListComponent<std::string>>(window, _("VIDEO OUTPUT"), false);
		std::string currentDevice = RecalboxConf::get("global.videooutput");
		if (currentDevice.empty())
			currentDevice = "auto";

		const std::vector<std::string> availableVideo = SystemInterface::getAvailableVideoOutputDevices();

		bool vfound = false;
		for (auto it = availableVideo.begin(); it != availableVideo.end(); it++)
		{
			optionsVideo->add((*it), (*it), currentDevice == (*it));
			if (currentDevice == (*it))
				vfound = true;
		}
		if (vfound == false)
			optionsVideo->add(currentDevice, currentDevice, true);

		gui.addWithLabel(_("VIDEO OUTPUT"), optionsVideo);

		gui.addSaveFunc([window, optionsVideo, currentDevice] {
			if (currentDevice != optionsVideo->getSelected())
			{
				RecalboxConf::set("global.videooutput", optionsVideo->getSelected());
				RecalboxConf::saveRecalboxConf();
				window->pushGui(new GuiMsgBox(window, _("THE SYSTEM WILL NOW REBOOT"), _("OK"), [] {
					if (Platform::runRestartCommand() != 0)
						LOG(LogWarning) << "Reboot terminated with non-zero result!";
				}));
			}
		});

		auto overscan_enabled = std::make_shared<SwitchComponent>(window, Settings::getInstance()->getBool("Overscan"));
		gui.addWithLabel(_("OVERSCAN"), overscan_enabled);
		gui.addSaveFunc([overscan_enabled] {
			if (Settings::getInstance()->getBool("Overscan") != overscan_enabled->getState())
			{
				Settings::getInstance()->setBool("Overscan", overscan_enabled->getState());
				SystemInterface::setOverscan(overscan_enabled->getState());
			}
		});
	}

	void createConfigInput(GuiMenu& menu, Window* window);

	std::shared_ptr<OptionListComponent<std::string>> createRatioOptionList(Window* window, std::string configname)
	{
		auto ratio_choice = std::make_shared<OptionListComponent<std::string>>(window, _("GAME RATIO"), false);
		const std::string currentRatio = RecalboxConf::get(configname + ".ratio", "auto");
		for (const auto& ratio : LibretroRatio::ratioMap)
			ratio_choice->add(_(ratio.first.c_str()), ratio.second, currentRatio == ratio.second);
		return ratio_choice;
	}

	void popSystemConfigurationGui(Window* mWindow, SystemData* systemData, const std::string& previouslySelectedEmulator)
	{
		// The system configuration
		GuiSettings* systemConfiguration = new GuiSettings(mWindow, systemData->getFullName().c_str());

		// Emulator choice
		auto emu_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, "emulator", false);
		bool selected = false;
		std::string selectedEmulator;
		for (const auto& it : *systemData->getEmulators())
		{
			bool found;
			const std::string& curEmulatorName = it.first;
			if (!previouslySelectedEmulator.empty())
				found = (previouslySelectedEmulator == curEmulatorName); // We just changed the emulator
			else
				found = (RecalboxConf::get(systemData->getName() + ".emulator") == curEmulatorName);

			if (found)
				selectedEmulator = curEmulatorName;

			selected = selected || found;
			emu_choice->add(curEmulatorName, curEmulatorName, found);
		}

		emu_choice->add("default", "default", !selected);
		emu_choice->setSelectedChangedCallback([mWindow, systemConfiguration, systemData](std::string s) {
			GuiMenuEx::popSystemConfigurationGui(mWindow, systemData, s); // Recursive call
			delete systemConfiguration;
		});
		systemConfiguration->addWithLabel(_("Emulator"), emu_choice);

		// Core choice
		auto core_choice = std::make_shared<OptionListComponent<std::string>>(mWindow, _("Core"), false);
		selected = false;
		for (const auto& emulator : *systemData->getEmulators())
		{
			if (selectedEmulator == emulator.first)
			{
				for (const auto& core : *emulator.second)
				{
					const bool found = (RecalboxConf::get(systemData->getName() + ".core") == core);
					selected = selected || found;
					core_choice->add(core, core, found);
				}
			}
		}
		core_choice->add("default", "default", !selected);
		systemConfiguration->addWithLabel(_("Core"), core_choice);

		// Screen ratio choice
		auto ratio_choice = GuiMenuEx::createRatioOptionList(mWindow, systemData->getName());
		systemConfiguration->addWithLabel(_("GAME RATIO"), ratio_choice);

		auto smoothing_enabled = std::make_shared<SwitchComponent>(mWindow, RecalboxConf::get(systemData->getName() + ".smooth", RecalboxConf::get("global.smooth")) == "1");
		systemConfiguration->addWithLabel(_("SMOOTH GAMES"), smoothing_enabled);

		auto rewind_enabled = std::make_shared<SwitchComponent>(mWindow, RecalboxConf::get(systemData->getName() + ".rewind", RecalboxConf::get("global.rewind")) == "1");
		systemConfiguration->addWithLabel(_("REWIND"), rewind_enabled);

		auto autosave_enabled = std::make_shared<SwitchComponent>(mWindow, RecalboxConf::get(systemData->getName() + ".autosave", RecalboxConf::get("global.autosave")) == "1");
		systemConfiguration->addWithLabel(_("AUTO SAVE/LOAD"), autosave_enabled);

		systemConfiguration->addSaveFunc([systemData, smoothing_enabled, rewind_enabled, ratio_choice, emu_choice, core_choice, autosave_enabled] {
			if (ratio_choice->changed())
				RecalboxConf::set(systemData->getName() + ".ratio", ratio_choice->getSelected());
			if (rewind_enabled->changed())
				RecalboxConf::set(systemData->getName() + ".rewind", rewind_enabled->getState() ? "1" : "0");
			if (smoothing_enabled->changed())
				RecalboxConf::set(systemData->getName() + ".smooth", smoothing_enabled->getState() ? "1" : "0");
			if (emu_choice->changed())
				RecalboxConf::set(systemData->getName() + ".emulator", emu_choice->getSelected());
			if (core_choice->changed())
				RecalboxConf::set(systemData->getName() + ".core", core_choice->getSelected());
			if (autosave_enabled->changed())
				RecalboxConf::set(systemData->getName() + ".autosave", autosave_enabled->getState() ? "1" : "0");
			RecalboxConf::saveRecalboxConf();
		});
		mWindow->pushGui(systemConfiguration);
	}

	void clearLoadedInput(std::vector<GuiMenu::StrInputConfig*>& v)
	{
		for (auto item : v)
			delete item;
		v.clear();
	}

	void AddAutoScrape(GuiSettings& gui, Window* window)
	{
		std::function<void()> handler = [window] {
			window->pushGui(
				new GuiMsgBox(window, _("REALLY SCRAPE?"), _("YES"), [window] { window->pushGui(new GuiAutoScrape(window)); }, _("NO"), nullptr));
		};

		ComponentListRow row;
		row.makeAcceptInputHandler(handler);
		auto sc_auto = std::make_shared<TextComponent>(window, _("AUTOMATIC SCRAPER"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
		auto bracket = makeArrow(window);
		row.addElement(sc_auto, false);
		gui.addRow(row);
	}

	void AddMenuScrape(GuiSettings& gui, Window* window, std::function<void()> handler)
	{
		ComponentListRow row;
		row.makeAcceptInputHandler(handler);
		auto sc_manual = std::make_shared<TextComponent>(window, _("MANUAL SCRAPER"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
		auto bracket = makeArrow(window);
		row.addElement(sc_manual, false);
		gui.addRow(row);
	}
} // namespace GuiMenuEx

void GuiMenuEx::createInputTextRow(Window* window, GuiSettings* gui, std::string title, const char* settingsID, bool password)
{
	ComponentListRow row;

	const auto label = std::make_shared<TextComponent>(window, title, Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
	row.addElement(label, true);

	std::shared_ptr<GuiComponent> ed;

	ed = std::make_shared<TextComponent>(window, ((password && !RecalboxConf::get(settingsID).empty()) ? "*********" : RecalboxConf::get(settingsID)),
		Font::get(FONT_SIZE_MEDIUM, FONT_PATH_LIGHT), COLOR_GRAY3, ALIGN_RIGHT);
	row.addElement(ed, true);

	auto spacer = std::make_shared<GuiComponent>(window);
	spacer->setSize(Renderer::getScreenWidth() * 0.005f, 0);
	row.addElement(spacer, false);

	auto bracket = std::make_shared<ImageComponent>(window);
	bracket->setImage(":/arrow.svg");
	bracket->setResize(Eigen::Vector2f(0, label->getFont()->getLetterHeight()));
	row.addElement(bracket, false);

	auto updateVal = [ed, settingsID, password](const std::string& newVal) {
		ed->setValue(!password ? newVal : "*********");
		RecalboxConf::set(settingsID, newVal);

	}; // ok callback (apply new value to ed)

	row.makeAcceptInputHandler([title, updateVal, settingsID, window] {
		if (Settings::getInstance()->getBool("UseOSK"))
			window->pushGui(new GuiTextEditPopupKeyboard(window, title, RecalboxConf::get(settingsID), updateVal, false));
		else
			window->pushGui(new GuiTextEditPopup(window, title, RecalboxConf::get(settingsID), updateVal, false));
	});
	gui->addRow(row);
}

void GuiMenuEx::AddMenuItems(GuiMenu& menu, Window* window)
{
	if (RecalboxConf::get("kodi.enabled") == "1")
	{
		menu.addEntry(_("KODI MEDIA CENTER").c_str(), COLOR_GRAY3, true, [window] {
			if (!SystemInterface::launchKodi(window))
				LOG(LogWarning) << "Shutdown terminated with non-zero result!";
		});
	}

#if defined(OBSOLETE)
	if (Settings::getInstance()->getBool("RomsManager"))
	{
		addEntry("ROMS MANAGER", COLOR_GRAY3, true, [this] { mWindow->pushGui(new GuiRomsManager(mWindow)); });
	}
#endif
#if defined(MENU_SYSTEM_SETTINGS_ENABLED)
	if (!IsBartopOptionEnabled())
	{
		menu.addEntry(_("SYSTEM SETTINGS").c_str(), COLOR_GRAY3, true, [window] {
			auto s = new GuiSettings(window, _("SYSTEM SETTINGS").c_str());

			// system informations
			{
				ComponentListRow row;
				std::function<void()> openGui = [window] {
					GuiSettings* informationsGui = new GuiSettings(window, _("INFORMATION").c_str());

					auto version = std::make_shared<TextComponent>(window, SystemInterface::getVersion(), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
					informationsGui->addWithLabel(_("VERSION"), version);
					bool warning = SystemInterface::isFreeSpaceLimit();
					auto space = std::make_shared<TextComponent>(
						window, SystemInterface::getFreeSpaceInfo(), Font::get(FONT_SIZE_MEDIUM), warning ? 0xFF0000FF : COLOR_GRAY3);
					informationsGui->addWithLabel(_("DISK USAGE"), space);

					// various informations
					const std::vector<std::string> infos = SystemInterface::getSystemInformations();
					for (const auto& it : infos)
					{
						std::vector<std::string> tokens;
						boost::split(tokens, it, boost::is_any_of(":"));
						if (tokens.size() >= 2)
						{
							// concatenate the ending words
							std::string vname = "";
							for (unsigned int i = 1; i < tokens.size(); i++)
							{
								if (i > 1)
									vname += " ";
								vname += tokens.at(i);
							}

							auto space = std::make_shared<TextComponent>(window, vname, Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
							informationsGui->addWithLabel(tokens.at(0), space);
						}
					}

					// support
					{
						ComponentListRow row;
						row.makeAcceptInputHandler([window] {
							window->pushGui(new GuiMsgBox(window, _("CREATE A SUPPORT FILE ?"), _("YES"),
								[window] {
									window->pushGui(new GuiMsgBox(window,
										SystemInterface::generateSupportFile() ? _("FILE GENERATED SUCCESSFULLY") : _("FILE GENERATION FAILED"),
										_("OK")));
								},
								_("NO"), nullptr));
						});
						auto supportFile =
							std::make_shared<TextComponent>(window, _("CREATE A SUPPORT FILE"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
						row.addElement(supportFile, false);
						informationsGui->addRow(row);
					}

					window->pushGui(informationsGui);
				};
				row.makeAcceptInputHandler(openGui);
				auto informationsSettings = std::make_shared<TextComponent>(window, _("INFORMATION"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
				auto bracket = makeArrow(window);
				row.addElement(informationsSettings, true);
				row.addElement(bracket, false);
				s->addRow(row);
			}

			const std::vector<std::string> availableStorage = SystemInterface::getAvailableStorageDevices();
			const std::string selectedStorage = SystemInterface::getCurrentStorage();

			// Storage device
			auto optionsStorage = std::make_shared<OptionListComponent<std::string>>(window, _("STORAGE DEVICE"), false);
			for (const auto& it : availableStorage)
			{
				if (it != "RAM")
				{
					if (boost::starts_with(it, "DEV"))
					{
						std::vector<std::string> tokens;
						boost::split(tokens, it, boost::is_any_of(" "));
						if (tokens.size() >= 3)
						{
							// concatenate the ending words
							std::string vname = "";
							for (unsigned int i = 2; i < tokens.size(); i++)
							{
								if (i > 2)
									vname += " ";
								vname += tokens.at(i);
							}
							optionsStorage->add(vname, it, selectedStorage == std::string("DEV " + tokens.at(1)));
						}
					}
					else
					{
						optionsStorage->add(it, it, selectedStorage == it);
					}
				}
			}
			s->addWithLabel(_("STORAGE DEVICE"), optionsStorage);

			// language choice
			auto language_choice = std::make_shared<OptionListComponent<std::string>>(window, _("LANGUAGE"), false);
			const std::string language = RecalboxConf::get("system.language", "en_US");
#if defined(WIN32) || defined(_WIN32)
#pragma warning(disable : 4566)
#endif
			language_choice->add("BASQUE", "eu_ES", language == "eu_ES");
			language_choice->add("正體中文", "zh_TW", language == "zh_TW");
			language_choice->add("简体中文", "zh_CN", language == "zh_CN");
			language_choice->add("DEUTSCH", "de_DE", language == "de_DE");
			language_choice->add("ENGLISH", "en_US", language == "en_US");
			language_choice->add("ESPAÑOL", "es_ES", language == "es_ES");
			language_choice->add("FRANÇAIS", "fr_FR", language == "fr_FR");
			language_choice->add("ITALIANO", "it_IT", language == "it_IT");
			language_choice->add("PORTUGUES BRASILEIRO", "pt_BR", language == "pt_BR");
			language_choice->add("PORTUGUES PORTUGAL", "pt_PT", language == "pt_PT");
			language_choice->add("SVENSKA", "sv_SE", language == "sv_SE");
			language_choice->add("TÜRKÇE", "tr_TR", language == "tr_TR");
			language_choice->add("CATALÀ", "ca_ES", language == "ca_ES");
			language_choice->add("ARABIC", "ar_YE", language == "ar_YE");
			language_choice->add("DUTCH", "nl_NL", language == "nl_NL");
			language_choice->add("GREEK", "el_GR", language == "el_GR");
			language_choice->add("KOREAN", "ko_KR", language == "ko_KR");
			language_choice->add("NORWEGIAN", "nn_NO", language == "nn_NO");
			language_choice->add("NORWEGIAN BOKMAL", "nb_NO", language == "nb_NO");
			language_choice->add("POLISH", "pl_PL", language == "pl_PL");
			language_choice->add("JAPANESE", "ja_JP", language == "ja_JP");
			language_choice->add("RUSSIAN", "ru_RU", language == "ru_RU");
			language_choice->add("HUNGARIAN", "hu_HU", language == "hu_HU");
#if defined(WIN32) || defined(_WIN32)
#pragma warning(default : 4566)
#endif
			s->addWithLabel(_("LANGUAGE"), language_choice);

			// Overclock choice
			auto overclock_choice = std::make_shared<OptionListComponent<std::string>>(window, _("OVERCLOCK"), false);
#if defined(RPI_VERSION)
#if RPI_VERSION == 1
			std::string currentOverclock = Settings::getInstance()->getString("Overclock");
			overclock_choice->add(_("EXTREM (1100Mhz)"), "extrem", currentOverclock == "extrem");
			overclock_choice->add(_("TURBO (1000Mhz)"), "turbo", currentOverclock == "turbo");
			overclock_choice->add(_("HIGH (950Mhz)"), "high", currentOverclock == "high");
			overclock_choice->add(_("NONE"), "none", currentOverclock == "none");
#elif RPI_VERSION == 2
            std::string currentOverclock = Settings::getInstance()->getString("Overclock");
            //overclock_choice->add(_("EXTREM (1100Mhz)"), "rpi2-extrem", currentOverclock == "rpi2-extrem");
            //overclock_choice->add(_("TURBO (1050Mhz)+"), "rpi2-turbo", currentOverclock == "rpi2-turbo");
            overclock_choice->add(_("HIGH (1050Mhz)"), "rpi2-high", currentOverclock == "rpi2-high");
            overclock_choice->add(_("NONE (900Mhz)"), "none", currentOverclock == "none");
#elif RPI_VERSION == 3
            std::string currentOverclock = Settings::getInstance()->getString("Overclock");
            overclock_choice->add(_("EXTREM (1350Mhz)"), "rpi3-extrem", currentOverclock == "rpi3-extrem");
            overclock_choice->add(_("TURBO (1325Mhz)+"), "rpi3-turbo", currentOverclock == "rpi3-turbo");
            overclock_choice->add(_("HIGH (1300Mhz)"), "rpi3-high", currentOverclock == "rpi3-high");
            overclock_choice->add(_("NONE (1200Mhz)"), "none", true);
#endif
#else
            overclock_choice->add(_("NONE"), "none", true);
#endif

			s->addWithLabel(_("OVERCLOCK"), overclock_choice);

			// Updates
			{
				ComponentListRow row;
				std::function<void()> openGuiD = [window] {
					GuiSettings* updateGui = new GuiSettings(window, _("UPDATES").c_str());

					auto updates_enabled = std::make_shared<SwitchComponent>(window, RecalboxConf::get("updates.enabled") == "1");
					updateGui->addWithLabel(_("AUTO UPDATES"), updates_enabled);

					// Start update
					{
						ComponentListRow updateRow;
						updateRow.makeAcceptInputHandler([window] { window->pushGui(new GuiUpdate(window)); });
						auto update = std::make_shared<TextComponent>(window, _("START UPDATE"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
						updateRow.addElement(update, true);
						updateRow.addElement(makeArrow(window), false); // bracket
						updateGui->addRow(updateRow);
					}
					updateGui->addSaveFunc([updates_enabled] {
						RecalboxConf::set("updates.enabled", updates_enabled->getState() ? "1" : "0");
						RecalboxConf::saveRecalboxConf();
					});
					window->pushGui(updateGui);

				};
				row.makeAcceptInputHandler(openGuiD);
				auto update = std::make_shared<TextComponent>(window, _("UPDATES"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
				row.addElement(update, true);
				row.addElement(makeArrow(window), false); // bracket
				s->addRow(row);
			}

			// backup
			{
				ComponentListRow row;
				row.makeAcceptInputHandler([window] { window->pushGui(new GuiBackupStart(window)); });
				auto backupSettings = std::make_shared<TextComponent>(window, _("BACKUP USER DATA"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
				row.addElement(backupSettings, true);
				row.addElement(makeArrow(window), false); // bracket
				s->addRow(row);
			}

			// Kodi
			{
				ComponentListRow row;
				std::function<void()> openGui = [window] {
					GuiSettings* kodiGui = new GuiSettings(window, _("KODI SETTINGS").c_str());
					auto kodiEnabled = std::make_shared<SwitchComponent>(window, RecalboxConf::get("kodi.enabled") == "1");
					kodiGui->addWithLabel(_("ENABLE KODI"), kodiEnabled);
					auto kodiAtStart = std::make_shared<SwitchComponent>(window, RecalboxConf::get("kodi.atstartup") == "1");
					kodiGui->addWithLabel(_("KODI AT START"), kodiAtStart);
					auto kodiX = std::make_shared<SwitchComponent>(window, RecalboxConf::get("kodi.xbutton") == "1");
					kodiGui->addWithLabel(_("START KODI WITH X"), kodiX);
					kodiGui->addSaveFunc([kodiEnabled, kodiAtStart, kodiX] {
						RecalboxConf::set("kodi.enabled", kodiEnabled->getState() ? "1" : "0");
						RecalboxConf::set("kodi.atstartup", kodiAtStart->getState() ? "1" : "0");
						RecalboxConf::set("kodi.xbutton", kodiX->getState() ? "1" : "0");
						RecalboxConf::saveRecalboxConf();
					});
					window->pushGui(kodiGui);
				};
				row.makeAcceptInputHandler(openGui);
				auto kodiSettings = std::make_shared<TextComponent>(window, _("KODI SETTINGS"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
				row.addElement(kodiSettings, true);
				row.addElement(makeArrow(window), false); // bracket
				s->addRow(row);
			}

			// install
			{
				ComponentListRow row;
				row.makeAcceptInputHandler([window] { window->pushGui(new GuiInstallStart(window)); });
				auto component = std::make_shared<TextComponent>(window, _("INSTALL ON A NEW DISK"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
				row.addElement(component, true);
				row.addElement(makeArrow(window), false); // bracket
				s->addRow(row);
			}

			// Security
			{
				ComponentListRow row;
				std::function<void()> openGui = [window] {
					GuiSettings* securityGui = new GuiSettings(window, _("SECURITY").c_str());
					auto securityEnabled = std::make_shared<SwitchComponent>(window, RecalboxConf::get("system.security.enabled") == "1");
					securityGui->addWithLabel(_("ENFORCE SECURITY"), securityEnabled);

					auto rootpassword =
						std::make_shared<TextComponent>(window, SystemInterface::getRootPassword(), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
					securityGui->addWithLabel(_("ROOT PASSWORD"), rootpassword);

					securityGui->addSaveFunc([window, securityEnabled] {
						bool reboot = false;

						if (securityEnabled->changed())
						{
							RecalboxConf::set("system.security.enabled", securityEnabled->getState() ? "1" : "0");
							RecalboxConf::saveRecalboxConf();
							reboot = true;
						}

						if (reboot)
						{
							window->pushGui(new GuiMsgBox(window, _("THE SYSTEM WILL NOW REBOOT"), _("OK"), [window] {
								if (Platform::runRestartCommand() != 0)
									LOG(LogWarning) << "Reboot terminated with non-zero result!";
							}));
						}
					});
					window->pushGui(securityGui);
				};
				row.makeAcceptInputHandler(openGui);
				auto securitySettings = std::make_shared<TextComponent>(window, _("SECURITY"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
				row.addElement(securitySettings, true);
				row.addElement(makeArrow(window), false); // bracket
				s->addRow(row);
			}

			s->addSaveFunc([overclock_choice, window, language_choice, language, optionsStorage, selectedStorage] {
				bool reboot = false;
				if (optionsStorage->changed())
				{
					SystemInterface::setStorage(optionsStorage->getSelected());
					reboot = true;
				}
				if (Settings::getInstance()->getString("Overclock") != overclock_choice->getSelected())
				{
					Settings::getInstance()->setString("Overclock", overclock_choice->getSelected());
					SystemInterface::setOverclock(overclock_choice->getSelected());
					reboot = true;
				}
				if (language != language_choice->getSelected())
				{
					RecalboxConf::set("system.language", language_choice->getSelected());
					RecalboxConf::saveRecalboxConf();
					reboot = true;
				}
				if (reboot)
				{
					window->pushGui(new GuiMsgBox(window, _("THE SYSTEM WILL NOW REBOOT"), _("OK"), [window] {
						if (Platform::runRestartCommand() != 0)
							LOG(LogWarning) << "Reboot terminated with non-zero result!";
					}));
				}
			});
			window->pushGui(s);
		});
	}
#endif
#if defined(MENU_GAMES_SETTINGS_ENABLED)
	menu.addEntry(_("GAMES SETTINGS"), COLOR_GRAY3, true, [&menu, window] {
		auto s = new GuiSettings(window, _("GAMES SETTINGS").c_str());
		if (!IsBartopOptionEnabled())
		{
			// Screen ratio choice
			auto ratio_choice = createRatioOptionList(window, "global");
			s->addWithLabel(_("GAME RATIO"), ratio_choice);
			s->addSaveFunc([ratio_choice] {
				RecalboxConf::set("global.ratio", ratio_choice->getSelected());
				RecalboxConf::saveRecalboxConf();
			});
		}
		auto smoothing_enabled = std::make_shared<SwitchComponent>(window, RecalboxConf::get("global.smooth") == "1");
		s->addWithLabel(_("SMOOTH GAMES"), smoothing_enabled);

		auto rewind_enabled = std::make_shared<SwitchComponent>(window, RecalboxConf::get("global.rewind") == "1");
		s->addWithLabel(_("REWIND"), rewind_enabled);

		auto autosave_enabled = std::make_shared<SwitchComponent>(window, RecalboxConf::get("global.autosave") == "1");
		s->addWithLabel(_("AUTO SAVE/LOAD"), autosave_enabled);

		// Shaders preset
		auto shaders_choices = std::make_shared<OptionListComponent<std::string>>(window, _("SHADERS SET"), false);
		const std::string currentShader = RecalboxConf::get("global.shaderset", "none");
		shaders_choices->add(_("NONE"), "none", currentShader == "none");
		shaders_choices->add(_("SCANLINES"), "scanlines", currentShader == "scanlines");
		shaders_choices->add(_("RETRO"), "retro", currentShader == "retro");
		s->addWithLabel(_("SHADERS SET"), shaders_choices);
		// Integer scale
		auto integerscale_enabled = std::make_shared<SwitchComponent>(window, RecalboxConf::get("global.integerscale") == "1");
		s->addWithLabel(_("INTEGER SCALE (PIXEL PERFECT)"), integerscale_enabled);
		s->addSaveFunc([integerscale_enabled] {
			RecalboxConf::set("global.integerscale", integerscale_enabled->getState() ? "1" : "0");
			RecalboxConf::saveRecalboxConf();
		});

		shaders_choices->setSelectedChangedCallback(
			[integerscale_enabled](const std::string& selectedShader) { integerscale_enabled->setState(selectedShader != "none"); });

		if (!IsBartopOptionEnabled())
		{
			// Retroachievements
			{
				ComponentListRow row;
				std::function<void()> openGui = [window] {
					GuiSettings* retroachievements = new GuiSettings(window, _("RETROACHIEVEMENTS SETTINGS").c_str());

					const auto retroachievements_enabled = std::make_shared<SwitchComponent>(window, RecalboxConf::get("global.retroachievements") == "1");
					retroachievements->addWithLabel(_("RETROACHIEVEMENTS"), retroachievements_enabled);

					const auto retroachievements_hardcore_enabled = std::make_shared<SwitchComponent>(window, RecalboxConf::get("global.retroachievements.hardcore") == "1");
					retroachievements->addWithLabel(_("HARDCORE MODE"), retroachievements_hardcore_enabled);

					GuiMenuEx::createInputTextRow(window, retroachievements, _("USERNAME"), "global.retroachievements.username", false);
					GuiMenuEx::createInputTextRow(window, retroachievements, _("PASSWORD"), "global.retroachievements.password", true);

					retroachievements->addSaveFunc([retroachievements_enabled, retroachievements_hardcore_enabled] {
						RecalboxConf::set("global.retroachievements", retroachievements_enabled->getState() ? "1" : "0");
						RecalboxConf::set("global.retroachievements.hardcore", retroachievements_hardcore_enabled->getState() ? "1" : "0");
						RecalboxConf::saveRecalboxConf();
					});
					window->pushGui(retroachievements);
				};
				row.makeAcceptInputHandler(openGui);
				auto retroachievementsSettings =
					std::make_shared<TextComponent>(window, _("RETROACHIEVEMENTS SETTINGS"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
				auto bracket = makeArrow(window);
				row.addElement(retroachievementsSettings, true);
				row.addElement(bracket, false);
				s->addRow(row);
			}
			// BIOS
			{
				std::function<void()> openGuiD = [window, s] {
					GuiSettings* configuration = new GuiSettings(window, _("MISSING BIOS").c_str());
					const std::vector<SystemInterface::BiosSystem> biosInformations = SystemInterface::getBiosInformations();

					if (biosInformations.size() == 0)
					{
						ComponentListRow noRow;
						auto biosText =
							std::make_shared<TextComponent>(window, _("NO MISSING BIOS").c_str(), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
						noRow.addElement(biosText, true);
						configuration->addRow(noRow);
					}
					else
					{
						for (const auto& systemBios : biosInformations)
						{
							ComponentListRow biosRow;
							auto biosText = std::make_shared<TextComponent>(window, systemBios.name.c_str(), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
							std::function<void()> openGuiDBios = [window, systemBios] {
								GuiSettings* configurationInfo = new GuiSettings(window, systemBios.name.c_str());
								for (const auto& biosFile : systemBios.bios)
								{
									auto biosPath = std::make_shared<TextComponent>(window, biosFile.path.c_str(), Font::get(FONT_SIZE_MEDIUM), COLOR_BLACK);
									auto biosMd5 = std::make_shared<TextComponent>(window, biosFile.md5.c_str(), Font::get(FONT_SIZE_SMALL), COLOR_GRAY3);
									auto biosStatus = std::make_shared<TextComponent>(window, biosFile.status.c_str(), Font::get(FONT_SIZE_SMALL), COLOR_GRAY3);
									ComponentListRow biosFileRow;
									biosFileRow.addElement(biosPath, true);
									configurationInfo->addRow(biosFileRow);
									configurationInfo->addWithLabel(_("MD5"), biosMd5);
									configurationInfo->addWithLabel(_("STATUS"), biosStatus);
								}
								window->pushGui(configurationInfo);
							};
							biosRow.makeAcceptInputHandler(openGuiDBios);
							auto bracket = makeArrow(window);
							biosRow.addElement(biosText, true);
							biosRow.addElement(bracket, false);
							configuration->addRow(biosRow);
						}
					}
					window->pushGui(configuration);
				};
				// bios button
				ComponentListRow row;
				row.makeAcceptInputHandler(openGuiD);
				auto bios = std::make_shared<TextComponent>(window, _("MISSING BIOS"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
				auto bracket = makeArrow(window);
				row.addElement(bios, true);
				row.addElement(bracket, false);
				s->addRow(row);
			}
			// Custom config for systems
			{
				ComponentListRow row;
				std::function<void()> openGuiD = [&menu, window, s] {
					s->save();
					GuiSettings* configuration = new GuiSettings(window, _("ADVANCED").c_str());
					// For each activated system
					std::vector<SystemData*> systems = SystemData::sSystemVector;
					for (auto system = systems.begin(); system != systems.end(); system++)
					{
						if ((*system) != SystemData::getFavoriteSystem())
						{
							ComponentListRow systemRow;
							auto systemText =
								std::make_shared<TextComponent>(window, (*system)->getFullName(), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
							auto bracket = makeArrow(window);
							systemRow.addElement(systemText, true);
							systemRow.addElement(bracket, false);
							SystemData* systemData = (*system);
							systemRow.makeAcceptInputHandler(
								[menu, systemData, window] { GuiMenuEx::popSystemConfigurationGui(window, systemData, ""); });
							configuration->addRow(systemRow);
						}
					}
					window->pushGui(configuration);

				};
				// Advanced button
				row.makeAcceptInputHandler(openGuiD);
				auto advanced = std::make_shared<TextComponent>(window, _("ADVANCED"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
				auto bracket = makeArrow(window);
				row.addElement(advanced, true);
				row.addElement(bracket, false);
				s->addRow(row);
			}
			// Game List Update
			{
				ComponentListRow row;

				row.makeAcceptInputHandler([window] {
					window->pushGui(new GuiMsgBox(window, _("REALLY UPDATE GAMES LISTS ?"), _("YES"),
						[window] {
							ViewController::get()->goToStart();
							window->renderShutdownScreen();
							delete ViewController::get();
							SystemData::deleteSystems();
							SystemData::loadConfig();
							GuiComponent* gui;
							while ((gui = window->peekGui()) != nullptr)
							{
								window->removeGui(gui);
								delete gui;
							}
							ViewController::init(window);
							ViewController::get()->reloadAll();
							window->pushGui(ViewController::get());
						},
						_("NO"), nullptr));
				});
				row.addElement(std::make_shared<TextComponent>(window, _("UPDATE GAMES LISTS"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3), true);
				s->addRow(row);
			}
		}
		s->addSaveFunc([smoothing_enabled, rewind_enabled, shaders_choices, autosave_enabled] {
			RecalboxConf::set("global.smooth", smoothing_enabled->getState() ? "1" : "0");
			RecalboxConf::set("global.rewind", rewind_enabled->getState() ? "1" : "0");
			RecalboxConf::set("global.shaderset", shaders_choices->getSelected());
			RecalboxConf::set("global.autosave", autosave_enabled->getState() ? "1" : "0");
			RecalboxConf::saveRecalboxConf();
		});
		window->pushGui(s);
	});
#endif
#if defined(MENU_CONTROLLERS_SETTINGS_ENABLED)
	if (!IsBartopOptionEnabled())
	{
		menu.addEntry(_("CONTROLLERS SETTINGS").c_str(), COLOR_GRAY3, true, [&menu, window] { createConfigInput(menu, window); });
	}
#endif
}

void GuiMenuEx::createConfigInput(GuiMenu& menu, Window* window)
{
	GuiSettings* s = new GuiSettings(window, _("CONTROLLERS SETTINGS"));

	ComponentListRow row;
	row.makeAcceptInputHandler([&menu, window, s] {
		window->pushGui(new GuiMsgBox(window,
			_("YOU ARE GOING TO CONFIGURE A CONTROLLER. IF YOU HAVE ONLY ONE JOYSTICK, "
			  "CONFIGURE THE DIRECTIONS KEYS AND SKIP JOYSTICK CONFIG BY HOLDING A BUTTON. "
			  "IF YOU DO NOT HAVE A SPECIAL KEY FOR HOTKEY, CHOOSE THE SELECT BUTTON. SKIP "
			  "ALL BUTTONS YOU DO NOT HAVE BY HOLDING A KEY. BUTTONS NAMES ARE BASED ON THE "
			  "SNES CONTROLLER."),
			_("OK"), [&menu, window, s] {
				window->pushGui(new GuiDetectDevice(window, false, [s, &menu, window] {
					s->setSave(false);
					delete s;
					createConfigInput(menu, window);
				}));
			}));
	});

	row.addElement(std::make_shared<TextComponent>(window, _("CONFIGURE A CONTROLLER"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3), true);
	s->addRow(row);

	row.elements.clear();

	std::function<void(void*)> showControllerList = [window, s](void* controllers) {
//         std::function<void(void *)> deletePairGui = [window](void *pairedPointer)
//         {
//             bool paired = *((bool*)pairedPointer);
//             window->pushGui(new GuiMsgBox(window, paired ? _("CONTROLLER PAIRED") : _("UNABLE TO PAIR CONTROLLER"), _("OK")));
//         };
		if (controllers == NULL)
		{
			window->pushGui(new GuiMsgBox(window, _("AN ERROR OCCURED"), _("OK")));
		}
		else
		{
			std::vector<std::string>* resolvedControllers = ((std::vector<std::string>*)controllers);
			if (resolvedControllers->size() == 0)
			{
				window->pushGui(new GuiMsgBox(window, _("NO CONTROLLERS FOUND"), _("OK")));
			}
			else
			{
				GuiSettings* pairGui = new GuiSettings(window, _("PAIR A BLUETOOTH CONTROLLER"));
				for (std::vector<std::string>::iterator controllerString = ((std::vector<std::string>*)controllers)->begin();
					 controllerString != ((std::vector<std::string>*)controllers)->end(); ++controllerString)
				{
					ComponentListRow controllerRow;
					std::function<void()> pairController = [window, pairGui, controllerString] //, deletePairGui]
					{
#if defined(EXTENSION)
						window->pushGui(new GuiLoading(window,
							[controllerString] {
								bool paired = SystemInterface::pairBluetooth(*controllerString);
								return static_cast<void*>(new bool(true)); // Memory leak!?
							},
							[window](void* pairedPointer) {
								const bool paired = *((bool*)pairedPointer);
								window->pushGui(new GuiMsgBox(window, paired ? _("CONTROLLER PAIRED") : _("UNABLE TO PAIR CONTROLLER"), _("OK")));
								// delete pairedPointer; // TODO: Fix memory leak
							}));
#endif
					};
					controllerRow.makeAcceptInputHandler(pairController);
					auto update = std::make_shared<TextComponent>(window, *controllerString, Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
					auto bracket = makeArrow(window);
					controllerRow.addElement(update, true);
					controllerRow.addElement(bracket, false);
					pairGui->addRow(controllerRow);
				}
				window->pushGui(pairGui);
			}
		}

		// delete controllers; // TODO: FIX Memory leak
	};

	row.makeAcceptInputHandler([window, s, showControllerList] {
#if defined(EXTENSION)
		window->pushGui(new GuiLoading(window,
			[] {
				return reinterpret_cast<void*>(SystemInterface::scanBluetooth()); // Memory leak!
			},
			showControllerList));
#endif
	});

	row.addElement(std::make_shared<TextComponent>(window, _("PAIR A BLUETOOTH CONTROLLER"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3), true);
	s->addRow(row);
	row.elements.clear();

	row.makeAcceptInputHandler([window, s] {
		SystemInterface::forgetBluetoothControllers();
		window->pushGui(new GuiMsgBox(window, _("CONTROLLERS LINKS HAVE BEEN DELETED."), _("OK")));
	});
	row.addElement(std::make_shared<TextComponent>(window, _("FORGET BLUETOOTH CONTROLLERS"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3), true);
	s->addRow(row);
	row.elements.clear();

	row.elements.clear();

	// Here we go; for each player
	std::list<int> alreadyTaken = std::list<int>();

	GuiMenuEx::clearLoadedInput(menu.mLoadedInput); // clear current loaded inputs

	std::vector<std::shared_ptr<OptionListComponent<GuiMenu::StrInputConfig*>>> options;
	char strbuf[256];

	for (int player = 0; player < MAX_PLAYERS; player++)
	{
		std::stringstream sstm;
		sstm << "INPUT P" << player + 1;
		std::string confName = sstm.str() + "NAME";
		std::string confGuid = sstm.str() + "GUID";
		snprintf(strbuf, 256, _("INPUT P%i").c_str(), player + 1);

		LOG(LogInfo) << player + 1 << " " << confName << " " << confGuid;
		auto inputOptionList = std::make_shared<OptionListComponent<GuiMenu::StrInputConfig*>>(window, strbuf, false);
		options.push_back(inputOptionList);

		// Checking if a setting has been saved, else setting to default
		std::string configuratedName = Settings::getInstance()->getString(confName);
		std::string configuratedGuid = Settings::getInstance()->getString(confGuid);
		bool found = false;
		// For each available and configured input
		for (auto it = 0; it < InputManager::getInstance()->getNumJoysticks(); it++)
		{
			InputConfig* config = InputManager::getInstance()->getInputConfigByDevice(it);
			if (config->isConfigured())
			{
				// create name
				std::stringstream dispNameSS;
				dispNameSS << "#" << config->getDeviceId() << " ";
				std::string deviceName = config->getDeviceName();
				if (deviceName.size() > 25)
				{
					dispNameSS << deviceName.substr(0, 16) << "..." << deviceName.substr(deviceName.size() - 5, deviceName.size() - 1);
				}
				else
				{
					dispNameSS << deviceName;
				}

				const std::string displayName = dispNameSS.str();

				bool foundFromConfig = (configuratedName == config->getDeviceName()) && (configuratedGuid == config->getDeviceGUID());
				int deviceID = config->getDeviceId();
				// Si la manette est configurée, qu'elle correspond a la configuration, et qu'elle n'est pas
				// deja selectionnée on l'ajoute en séléctionnée
				auto newInputConfig = new GuiMenu::StrInputConfig(config->getDeviceName(), config->getDeviceGUID());
				menu.mLoadedInput.push_back(newInputConfig);

				if (foundFromConfig && std::find(alreadyTaken.begin(), alreadyTaken.end(), deviceID) == alreadyTaken.end() && !found)
				{
					found = true;
					alreadyTaken.push_back(deviceID);
					LOG(LogWarning) << "adding entry for player" << player << " (selected): " << config->getDeviceName() << "  "
									<< config->getDeviceGUID();
					inputOptionList->add(displayName, newInputConfig, true);
				}
				else
				{
					LOG(LogWarning) << "adding entry for player" << player << " (not selected): " << config->getDeviceName() << "  "
									<< config->getDeviceGUID();
					inputOptionList->add(displayName, newInputConfig, false);
				}
			}
		}
		if (configuratedName.compare("") == 0 || !found)
		{
			LOG(LogWarning) << "adding default entry for player " << player << "(selected : true)";
			inputOptionList->add("default", NULL, true);
		}
		else
		{
			LOG(LogWarning) << "adding default entry for player" << player << "(selected : false)";
			inputOptionList->add("default", NULL, false);
		}

		// ADD default config

		// Populate controllers list
		s->addWithLabel(strbuf, inputOptionList);
	}

	s->addSaveFunc([options, window] {
		for (int player = 0; player < MAX_PLAYERS; player++)
		{
			std::stringstream sstm;
			sstm << "INPUT P" << player + 1;
			std::string confName = sstm.str() + "NAME";
			std::string confGuid = sstm.str() + "GUID";

			auto input_p1 = options.at(player);
			std::string name;
			std::string selectedName = input_p1->getSelectedName();

			if (selectedName.compare(strToUpper("default")) == 0)
			{
				name = "DEFAULT";
				Settings::getInstance()->setString(confName, name);
				Settings::getInstance()->setString(confGuid, "");
			}
			else
			{
				if (input_p1->getSelected() != NULL)
				{
					LOG(LogWarning) << "Found the selected controller ! : name in list  = " << selectedName;
					LOG(LogWarning) << "Found the selected controller ! : guid  = " << input_p1->getSelected()->deviceGUIDString;

					Settings::getInstance()->setString(confName, input_p1->getSelected()->deviceName);
					Settings::getInstance()->setString(confGuid, input_p1->getSelected()->deviceGUIDString);
				}
			}
		}

		Settings::getInstance()->saveFile();
	});

	row.elements.clear();
	window->pushGui(s);
}

void GuiMenuEx::AddMenuNetwork(GuiMenu& menu, Window* window)
{
#if defined(MENU_NETWORK_SETTINGS_ENABLED)
	if (!IsBartopOptionEnabled())
	{
		menu.addEntry(_("NETWORK SETTINGS").c_str(), COLOR_GRAY3, true, [window] {
			auto s = new GuiSettings(window, _("NETWORK SETTINGS").c_str());
			auto status = std::make_shared<TextComponent>(
				window, SystemInterface::ping() ? _("CONNECTED") : _("NOT CONNECTED"), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
			s->addWithLabel(_("STATUS"), status);
			auto ip = std::make_shared<TextComponent>(window, SystemInterface::getIpAdress(), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);
			s->addWithLabel(_("IP ADDRESS"), ip);
			// Hostname
			GuiMenuEx::createInputTextRow(window, s, _("HOSTNAME"), "system.hostname", false);

			// Wifi enable
			const bool baseEnabled = (RecalboxConf::get("wifi.enabled") == "1");
			auto enable_wifi = std::make_shared<SwitchComponent>(window, baseEnabled);
			s->addWithLabel(_("ENABLE WIFI"), enable_wifi);

			// window, title, settingstring,
			const std::string baseSSID = RecalboxConf::get("wifi.ssid");
			GuiMenuEx::createInputTextRow(window, s, _("WIFI SSID"), "wifi.ssid", false);
			const std::string baseKEY = RecalboxConf::get("wifi.key");
			GuiMenuEx::createInputTextRow(window, s, _("WIFI KEY"), "wifi.key", true);

			s->addSaveFunc([baseEnabled, baseSSID, baseKEY, enable_wifi, window] {
				const bool wifienabled = enable_wifi->getState();
				RecalboxConf::set("wifi.enabled", wifienabled ? "1" : "0");
				const std::string newSSID = RecalboxConf::get("wifi.ssid");
				const std::string newKey = RecalboxConf::get("wifi.key");
				RecalboxConf::saveRecalboxConf();
				if (wifienabled)
				{
					if (baseSSID != newSSID || baseKEY != newKey || !baseEnabled)
					{
						window->pushGui(
							new GuiMsgBox(window, SystemInterface::enableWifi(newSSID, newKey) ? _("WIFI ENABLED") : _("WIFI CONFIGURATION ERROR")));
					}
				}
				else if (baseEnabled)
				{
					SystemInterface::disableWifi();
				}
			});
			window->pushGui(s);
		});
	}
#endif
}

#endif