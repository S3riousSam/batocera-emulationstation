#if defined(EXTENSION)
#include "AudioManager.h"
#if defined(EXTENSION) && defined(USEFUL)
#include "guis/GuiMsgBoxScroll.h"
#endif
#include "Log.h"
#include "Music.h"
#include "NetworkThread.h"
#include "RecalboxConf.h"
#include "Settings.h"
#include "SystemInterface.h"
#include "VolumeControl.h"
#include "Window.h"
#include "platform.h"
#include <boost/filesystem.hpp>
#include <boost/locale.hpp>
#ifdef WIN32
#include <Windows.h>
#include <direct.h> // getcwd, chdir
#define PATH_MAX 256
#endif

namespace Extension
{
	void initAudio()
	{
		VolumeControl::getInstance()->init();
		AudioManager::getInstance()->init();
	}

	void playSound(const std::string& name)
	{
		const std::string selectedTheme = Settings::getInstance()->getString("ThemeSet");
		const std::string loadingMusic = Platform::getHomePath() + "/.emulationstation/themes/" + selectedTheme + "/fx/" + name + ".ogg";
		if (boost::filesystem::exists(loadingMusic))
		{
			Music::get(loadingMusic)->play(false, NULL);
		}
	}

	int setLocale(char* argv1)
	{
		char path_save[PATH_MAX];
		char abs_exe_path[PATH_MAX];
		char* p = strrchr(argv1, '/');

#if !defined(WIN32)
#pragma GCC diagnostic push#pragma GCC diagnostic ignored "-Wunused-result"
#endif
		if (p == nullptr)
		{
			getcwd(abs_exe_path, sizeof(abs_exe_path));
		}
		else
		{
			*p = '\0';
			// http://pubs.opengroup.org/onlinepubs/009695399/functions/getcwd.html
			getcwd(path_save, sizeof(path_save));
			// http://pubs.opengroup.org/onlinepubs/009695399/functions/chdir.html
			chdir(argv1);
			getcwd(abs_exe_path, sizeof(abs_exe_path));
			chdir(path_save);
		}
#if !defined(WIN32)
#pragma GCC diagnostic pop
#endif

		boost::locale::localization_backend_manager my = boost::locale::localization_backend_manager::global();
		// Get global backend

		my.select("std");
		boost::locale::localization_backend_manager::global(my);
		// set this backend globally

		boost::locale::generator gen;

		std::string localeDir = abs_exe_path;
		localeDir += "/locale/lang";
		LOG(LogInfo) << "Setting local directory to " << localeDir;
		// Specify location of dictionaries
		gen.add_messages_path(localeDir);
		gen.add_messages_path("/usr/share/locale");
		gen.add_messages_domain("emulationstation2");

		// Generate locales and imbue them to iostream
		std::locale::global(gen(""));
		std::cout.imbue(std::locale());
		LOG(LogInfo) << "Locals set...";
		return 0;
	}

	void performExtra(Window& window)
	{
		if (RecalboxConf::get("kodi.enabled") == "1" && RecalboxConf::get("kodi.atstartup") == "1")
		{
			SystemInterface::launchKodi(&window);
		}

		SystemInterface::getIpAdress();
#if defined(USEFUL)
		// UPDATED VERSION MESSAGE
		// if(RecalboxSystem::getInstance()->needToShowVersionMessage())
		//{
		//	 window.pushGui(new GuiMsgBoxScroll(&window, RecalboxSystem::getInstance()->getVersionMessage(), "OK", [] {
		// RecalboxSystem::getInstance()->updateLastVersionFile(); },"",nullptr,"",nullptr, ALIGN_LEFT));
		//}
#endif
		if (RecalboxConf::get("updates.enabled") == "1")
		{
			NetworkThread* nthread = new NetworkThread(window); // TODO: memory leak?!
		}
	}

	void byebye(bool reboot, bool shutdown)
	{
#if !defined(WIN32)
#pragma GCC diagnostic push#pragma GCC diagnostic ignored "-Wunused-result"
#endif
		// http://pubs.opengroup.org/onlinepubs/009695399/functions/system.html
		if (reboot)
		{
			LOG(LogInfo) << "Rebooting system";
			system("touch /tmp/reboot.please");
			system("shutdown -r now");
		}
		else if (shutdown)
		{
			LOG(LogInfo) << "Shutting system down";
			system("touch /tmp/shutdown.please");
			system("shutdown -h now");
		}
#if !defined(WIN32)
#pragma GCC diagnostic pop
#endif
	}
} // namespace Extension
#endif