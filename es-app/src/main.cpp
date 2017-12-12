// EmulationStation, a graphical front-end for ROM browsing. Created by Alec "Aloshi" Lofquist.
// http://www.aloshi.com

#include "AudioManager.h"
#include "EmulationStation.h"
#include "Log.h"
#include "Renderer.h"
#include "ScraperCmdLine.h"
#include "Settings.h"
#include "SystemData.h"
#include "Window.h"
#include "guis/GuiDetectDevice.h"
#include "guis/GuiMsgBox.h"
#include "platform.h"
#include "views/ViewController.h"
#include <SDL.h>
#include <boost/filesystem.hpp>
#include <boost/locale.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#if defined(EXTENSION)
#include "FileSorts.h"
#include "LocaleES.h"
#include "SystemInterface.h"

namespace RecalBox
{
	void initAudio();
	void playSound(const std::string& name);
	int setLocale(char* argv1);

	void performExtra(Window& window);
	void byebye(bool reboot, bool shutdown);
} // namespace RecalBox

#else
#define _(A) A
#endif

#ifdef WIN32
#define _CRTDBG_MAP_ALLOC
#include <Windows.h>
#include <crtdbg.h>
#include <stdlib.h>
#endif

namespace fs = boost::filesystem;

bool scrape_cmdline = false;

bool parseArgs(int argc, char* argv[], unsigned int* width, unsigned int* height)
{
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--resolution") == 0)
		{
			if (i >= argc - 2)
			{
				std::cerr << "Invalid resolution supplied.";
				return false;
			}

			*width = atoi(argv[i + 1]);
			*height = atoi(argv[i + 2]);
			i += 2; // skip the argument value
		}
		else if (strcmp(argv[i], "--gamelist-only") == 0)
		{
			Settings::getInstance()->setBool("ParseGamelistOnly", true);
		}
		else if (strcmp(argv[i], "--ignore-gamelist") == 0)
		{
			Settings::getInstance()->setBool("IgnoreGamelist", true);
		}
		else if (strcmp(argv[i], "--draw-framerate") == 0)
		{
			Settings::getInstance()->setBool("DrawFramerate", true);
		}
		else if (strcmp(argv[i], "--no-exit") == 0)
		{
			Settings::getInstance()->setBool("ShowExit", false);
		}
		else if (strcmp(argv[i], "--debug") == 0)
		{
			Settings::getInstance()->setBool("Debug", true);
			Settings::getInstance()->setBool("HideConsole", false);
			Log::setReportingLevel(LogDebug);
		}
		else if (strcmp(argv[i], "--windowed") == 0)
		{
			Settings::getInstance()->setBool("Windowed", true);
		}
		else if (strcmp(argv[i], "--vsync") == 0)
		{
			bool vsync = (strcmp(argv[i + 1], "on") == 0 || strcmp(argv[i + 1], "1") == 0) ? true : false;
			Settings::getInstance()->setBool("VSync", vsync);
			i++; // skip vsync value
		}
		else if (strcmp(argv[i], "--scrape") == 0)
		{
			scrape_cmdline = true;
		}
		else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
		{
#ifdef WIN32
			// This is a bit of a hack, but otherwise output will go to nowhere
			// when the application is compiled with the "WINDOWS" subsystem (which we usually are).
			// If you're an experienced Windows programmer and know how to do this
			// the right way, please submit a pull request!
			AttachConsole(ATTACH_PARENT_PROCESS);
			freopen("CONOUT$", "wb", stdout);
#endif
			std::cout << "EmulationStation, a graphical front-end for ROM browsing.\n"
						 "Written by Alec \"Aloshi\" Lofquist.\n"
						 "Version "
					  << PROGRAM_VERSION_STRING << ", built " << PROGRAM_BUILT_STRING
					  << "\n\n"
						 "Command line arguments:\n"
						 "--resolution [width] [height]	try and force a particular resolution\n"
						 "--gamelist-only			skip automatic game search, only read from gamelist.xml\n"
						 "--ignore-gamelist		ignore the gamelist (useful for troubleshooting)\n"
						 "--draw-framerate		display the framerate\n"
						 "--no-exit			don't show the exit option in the menu\n"
						 "--debug				more logging, show console on Windows\n"
						 "--scrape			scrape using command line interface\n"
						 "--windowed			not fullscreen, should be used with --resolution\n"
						 "--vsync [1/on or 0/off]		turn vsync on or off (default is on)\n"
						 "--help, -h			summon a sentient, angry tuba\n\n"
						 "More information available in README.md.\n";
			return false; // exit after printing help
		}
	}

	return true;
}

bool verifyHomeFolderExists()
{
	// make sure the config directory exists
	std::string home = getHomePath();
	std::string configDir = home + "/.emulationstation";
	if (!fs::exists(configDir))
	{
		std::cout << "Creating config directory \"" << configDir << "\"\n";
		fs::create_directory(configDir);
		if (!fs::exists(configDir))
		{
			std::cerr << "Config directory could not be created!\n";
			return false;
		}
	}

	return true;
}

// Returns true if everything is OK,
bool loadSystemConfigFile(const char** errorString)
{
	*errorString = NULL;

	if (!SystemData::loadConfig())
	{
		LOG(LogError) << "Error while parsing systems configuration file!";
		*errorString =
			"IT LOOKS LIKE YOUR SYSTEMS CONFIGURATION FILE HAS NOT BEEN SET UP OR IS INVALID. YOU'LL NEED TO DO THIS BY HAND, UNFORTUNATELY.";
		return false;
	}

	if (SystemData::sSystemVector.size() == 0)
	{
		LOG(LogError) << "No systems found! Does at least one system have a game present? (check that extensions match!)\n(Also, make sure you've "
						 "updated your es_systems.cfg for XML!)";
		*errorString = "WE CAN'T FIND ANY SYSTEMS!\n"
					   "CHECK THAT YOUR PATHS ARE CORRECT IN THE SYSTEMS CONFIGURATION FILE, AND "
					   "YOUR GAME DIRECTORY HAS AT LEAST ONE GAME WITH THE CORRECT EXTENSION.";
		return false;
	}

	return true;
}

int main(int argc, char* argv[])
{
#if defined(WIN32)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	unsigned int width = 0;
	unsigned int height = 0;
#if !defined(EXTENSION)
	std::locale::global(boost::locale::generator().generate(""));
	boost::filesystem::path::imbue(std::locale());
#endif

	if (!parseArgs(argc, argv, &width, &height))
		return 0;

		// only show the console on Windows if HideConsole is false
#ifdef WIN32
	// MSVC has a "SubSystem" option, with two primary options: "WINDOWS" and "CONSOLE".
	// In "WINDOWS" mode, no console is automatically created for us.  This is good,
	// because we can choose to only create the console window if the user explicitly
	// asks for it, preventing it from flashing open and then closing.
	// In "CONSOLE" mode, a console is always automatically created for us before we
	// enter main. In this case, we can only hide the console after the fact, which
	// will leave a brief flash.
	// TL;DR: You should compile ES under the "WINDOWS" subsystem.
	// I have no idea how this works with non-MSVC compilers.
	if (!Settings::getInstance()->getBool("HideConsole"))
	{
		// we want to show the console
		// if we're compiled in "CONSOLE" mode, this is already done.
		// if we're compiled in "WINDOWS" mode, no console is created for us automatically;
		// the user asked for one, so make one and then hook stdin/stdout/sterr up to it
		if (AllocConsole()) // should only pass in "WINDOWS" mode
		{
			freopen("CONIN$", "r", stdin);
			freopen("CONOUT$", "wb", stdout);
			freopen("CONOUT$", "wb", stderr);
		}
	}
	else
	{
		// we want to hide the console
		// if we're compiled with the "WINDOWS" subsystem, this is already done.
		// if we're compiled with the "CONSOLE" subsystem, a console is already created;
		// it'll flash open, but we hide it nearly immediately
		if (GetConsoleWindow()) // should only pass in "CONSOLE" mode
			ShowWindow(GetConsoleWindow(), SW_HIDE);
	}
#endif

	// if ~/.emulationstation doesn't exist and cannot be created, bail
	if (!verifyHomeFolderExists())
		return 1;

	// start the logger
	Log::open();
	atexit(&Log::close); // Always close the log on exit
	LOG(LogInfo) << "EmulationStation - v" << PROGRAM_VERSION_STRING << ", built " << PROGRAM_BUILT_STRING;

#if defined(EXTENSION)
	RecalBox::setLocale(argv[0]);

	// FileSorts::init(); // require locale
	initMetadata(); // require locale

	Renderer::init(width, height);
#endif
	Window window;
	ViewController::init(&window);
	window.pushGui(ViewController::get());

	if (!scrape_cmdline)
	{
#if defined(EXTENSION)
		if (!window.init(width, height, false))
#else
		if (!window.init(width, height))
#endif
		{
			LOG(LogError) << "Window failed to initialize!";
			return 1;
		}

		const GLubyte* result = glGetString(GL_EXTENSIONS);
		if (result != NULL)
		{
			const std::string glExts = reinterpret_cast<const char*>(result);
			LOG(LogInfo) << "Checking available OpenGL extensions...";
			LOG(LogInfo) << " ARB_texture_non_power_of_two: " << (glExts.find("ARB_texture_non_power_of_two") != std::string::npos ? "ok" : "MISSING");
		}
		else
		{
			LOG(LogInfo) << "Checking available OpenGL extensions... FAILED";
		}

		window.renderLoadingScreen();
	}
#if defined(EXTENSION)
	RecalBox::initAudio();
	RecalBox::playSound("loading");
#endif
	const char* errorMsg = NULL;
	if (!loadSystemConfigFile(&errorMsg))
	{
		// something went terribly wrong
		if (errorMsg == NULL)
		{
			LOG(LogError) << "Unknown error occurred while parsing system config file.";
			if (!scrape_cmdline)
				Renderer::deinit();
			return 1;
		}

		// we can't handle es_systems.cfg file problems inside ES itself, so display the error message then quit
		window.pushGui(new GuiMsgBox(&window, errorMsg, _("QUIT"), [] {
			SDL_Event* quit = new SDL_Event();
			quit->type = SDL_QUIT;
			SDL_PushEvent(quit);
		}));
	}
#if defined(EXTENSION)
	RecalBox::performExtra(window);
#endif
	// run the command line scraper then quit
	if (scrape_cmdline)
	{
		return run_scraper_cmdline();
	}

	// dont generate joystick events while we're loading (hopefully fixes "automatically started emulator" bug)
	SDL_JoystickEventState(SDL_DISABLE);

	// preload what we can right away instead of waiting for the user to select it
	// this makes for no delays when accessing content, but a longer startup time
#if !defined(EXTENSION)
	ViewController::get()->preload();
#endif

	// choose which GUI to open depending on if an input configuration already exists
	if (errorMsg == NULL)
	{
		if (fs::exists(InputManager::getConfigPath()) && InputManager::getInstance()->getNumConfiguredDevices() > 0)
		{
			ViewController::get()->goToStart();
		}
		else
		{
			window.pushGui(new GuiDetectDevice(&window, true, [] { ViewController::get()->goToStart(); }));
		}
	}
#if defined(EXTENSION)
	// Create a flag in  temporary directory to signal READY state
	fs::path ready_path = fs::temp_directory_path();
	ready_path /= "emulationstation.ready";
	FILE* ready_file = fopen(ready_path.generic_string().c_str(), "w");
	if (ready_file)
		fclose(ready_file);
#endif
	// generate joystick events since we're done loading
	SDL_JoystickEventState(SDL_ENABLE);

	int lastTime = SDL_GetTicks();
	bool running = true;
#if defined(EXTENSION)
	bool doReboot = false;
	bool doShutdown = false;
#endif
	while (running)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_JOYHATMOTION:
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
			case SDL_KEYDOWN:
			case SDL_KEYUP:
			case SDL_JOYAXISMOTION:
			case SDL_TEXTINPUT:
			case SDL_TEXTEDITING:
			case SDL_JOYDEVICEADDED:
			case SDL_JOYDEVICEREMOVED:
				InputManager::getInstance()->parseEvent(event, &window);
				break;
			case SDL_QUIT:
				running = false;
				break;
#if defined(EXTENSION)
			case SystemInterface::SDL_FAST_QUIT | SystemInterface::SDL_RB_REBOOT:
				running = false;
				doReboot = true;
				Settings::getInstance()->setBool("IgnoreGamelist", true);
				break;
			case SystemInterface::SDL_FAST_QUIT | SystemInterface::SDL_RB_SHUTDOWN:
				running = false;
				doShutdown = true;
				Settings::getInstance()->setBool("IgnoreGamelist", true);
				break;
			case SDL_QUIT | SystemInterface::SDL_RB_REBOOT:
				running = false;
				doReboot = true;
				break;
			case SDL_QUIT | SystemInterface::SDL_RB_SHUTDOWN:
				running = false;
				doShutdown = true;
				break;
#endif
			}
		}

		if (window.isSleeping())
		{
			lastTime = SDL_GetTicks();
			SDL_Delay(1); // this doesn't need to be accurate, we're just giving up our CPU time until something wakes us up
			continue;
		}

		int curTime = SDL_GetTicks();
		int deltaTime = curTime - lastTime;
		lastTime = curTime;

		// cap deltaTime at 1000
		if (deltaTime > 1000 || deltaTime < 0)
			deltaTime = 1000;

		window.update(deltaTime);
		window.render();
		Renderer::swapBuffers();

		Log::flush();
	}
#if defined(EXTENSION)
	if (fs::exists(ready_path))
		fs::remove(ready_path); // Clean ready flag
#endif
	while (window.peekGui() != ViewController::get())
		delete window.peekGui();
#if !defined(EXTENSION)
	window.deinit();

	SystemData::deleteSystems();

#else
	window.renderShutdownScreen();
	SystemData::deleteSystems();
	window.deinit();
#endif
	LOG(LogInfo) << "EmulationStation cleanly shutting down.";
#if defined(EXTENSION)
	RecalBox::byebye(doReboot, doShutdown);
#endif

	Font::uinitLibrary();
	delete Settings::getInstance();
	delete InputManager::getInstance();

	return 0;
}
