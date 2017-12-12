#include "SystemData.h"
#include "AudioManager.h"
#include "FileSorts.h"
#include "Gamelist.h"
#include "InputManager.h"
#include "Log.h"
#include "Renderer.h"
#include "Settings.h"
#include "VolumeControl.h"
#include <SDL_joystick.h>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#if defined(EXTENSION) || !defined(EXTENSION)
#include "Util.h"
#include <boost/asio/io_service.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>
#include <utility>
#endif

std::vector<SystemData*> SystemData::sSystemVector;

namespace fs = boost::filesystem;

namespace
{
	void writeExampleConfig(const std::string& path)
	{
		std::ofstream file(path.c_str());

		file << "<!-- This is the EmulationStation Systems configuration file.\n"
				"All systems must be contained within the <systemList> tag.-->\n"
				"\n"
				"<systemList>\n"
				"	<!-- Here's an example system to get you started. -->\n"
				"	<system>\n"
				"\n"
				"		<!-- A short name, used internally. Traditionally lower-case. -->\n"
				"		<name>nes</name>\n"
				"\n"
				"		<!-- A \"pretty\" name, displayed in menus and such. -->\n"
				"		<fullname>Nintendo Entertainment System</fullname>\n"
				"\n"
				"		<!-- The path to start searching for ROMs in. '~' will be expanded to $HOME on Linux or %HOMEPATH% on Windows. -->\n"
				"		<path>~/roms/nes</path>\n"
				"\n"
				"		<!-- A list of extensions to search for, delimited by any of the whitespace characters (\", \\r\\n\\t\").\n"
				"		You MUST include the period at the start of the extension! It's also case sensitive. -->\n"
				"		<extension>.nes .NES</extension>\n"
				"\n"
				"		<!-- The shell command executed when a game is selected. A few special tags are replaced if found in a command:\n"
				"		%ROM% is replaced by a bash-special-character-escaped absolute path to the ROM.\n"
				"		%BASENAME% is replaced by the \"base\" name of the ROM.  For example, \"/foo/bar.rom\" would have a basename of \"bar\". "
				"Useful for MAME.\n"
				"		%ROM_RAW% is the raw, unescaped path to the ROM. -->\n"
				"		<command>retroarch -L ~/cores/libretro-fceumm.so %ROM%</command>\n"
				"\n"
				"		<!-- The platform to use when scraping. You can see the full list of accepted platforms in src/PlatformIds.cpp.\n"
				"		It's case sensitive, but everything is lowercase. This tag is optional.\n"
				"		You can use multiple platforms too, delimited with any of the whitespace characters (\", \\r\\n\\t\"), eg: \"genesis, "
				"megadrive\" -->\n"
				"		<platform>nes</platform>\n"
				"\n"
				"		<!-- The theme to load from the current theme set.  See THEMES.md for more information.\n"
				"		This tag is optional. If not set, it will default to the value of <name>. -->\n"
				"		<theme>nes</theme>\n"
				"	</system>\n"
				"</systemList>\n";

		file.close();

		LOG(LogError) << "Example config written!  Go read it at \"" << path << "\"!";
	}

	std::string GetStartPath(const std::string& path)
	{
		const std::string defaultRomsPath = getExpandedPath(Settings::getInstance()->getString("DefaultRomsPath"));
		return defaultRomsPath.empty() ? path : fs::absolute(path, defaultRomsPath).generic_string();
	}
} // namespace

SystemData::SystemData(const std::string& name, const std::string& fullName, const std::string& startPath, const std::vector<std::string>& extensions,
	const std::string& command, const std::vector<PlatformIds::PlatformId>& platformIds, const std::string& themeFolder
#if defined(EXTENSION)
	,
	std::map<std::string, std::vector<std::string>*>* emulators
#endif
	)
	: mName(name)
	, mFullName(fullName)
#if defined(EXTENSION)
	, mStartPath(GetStartPath(getExpandedPath(startPath)))
	, mEmulators(emulators)
#else
	, mStartPath(startPath)
#endif
	, mSearchExtensions(extensions)
	, mLaunchCommand(command)
	, mPlatformIds(platformIds)
	, mThemeFolder(themeFolder)
{
	mRootFolder = new FileData(FOLDER, mStartPath, this);
	mRootFolder->metadata.set("name", mFullName);

	if (!Settings::getInstance()->getBool("ParseGamelistOnly"))
		populateFolder(mRootFolder);

	if (!Settings::getInstance()->getBool("IgnoreGamelist"))
		parseGamelist(this);

	mRootFolder->sort(FileSorts::SortTypes.at(0));
#if defined(EXTENSION)
	mIsFavorite = false;
	loadTheme();
#endif
}

#if defined(EXTENSION)
SystemData::SystemData(const std::string& name, const std::string& fullName, const std::string& command, const std::string& themeFolder, std::vector<SystemData*>* systems)
	: mName(name)
	, mFullName(fullName)
	, mLaunchCommand(command)
	, mThemeFolder(themeFolder)
{
	mRootFolder = new FileData(FOLDER, mStartPath, this);
	mRootFolder->metadata.set("name", mFullName);

	for (const auto& system : *systems)
	{
		for (auto favorite : system->getFavorites())
			mRootFolder->addAlreadyExisitingChild(favorite);
	}

	if (mRootFolder->getChildren().size())
		mRootFolder->sort(FileSorts::SortTypes.at(0));
	mIsFavorite = true;
	mPlatformIds.push_back(PlatformIds::PLATFORM_IGNORE);
	loadTheme();
}
#endif

SystemData::~SystemData()
{
	// save changed game data back to xml
	if (!Settings::getInstance()->getBool("IgnoreGamelist"))
		updateGamelist(this);
#if defined(EXTENSION)
	for (const auto& it : *mEmulators)
		delete it.second;
	delete mEmulators;
#endif
	delete mRootFolder;
}

// plaform-specific escape path function
// on windows: just puts the path in quotes
// everything else: assume bash and escape special characters with backslashes
std::string escapePath(const boost::filesystem::path& path)
{
#ifdef WIN32
	// windows escapes stuff by just putting everything in quotes
	return '"' + fs::path(path).make_preferred().string() + '"';
#else
	// a quick and dirty way to insert a backslash before most characters that would mess up a bash path
	std::string pathStr = path.string();

	const char* invalidChars = " '\"\\!$^&*(){}[]?;<>";
	for (unsigned int i = 0; i < pathStr.length(); i++)
	{
		char c;
		unsigned int charNum = 0;
		do
		{
			c = invalidChars[charNum];
			if (pathStr[i] == c)
			{
				pathStr.insert(i, "\\");
				i++;
				break;
			}
			charNum++;
		} while (c != '\0');
	}

	return pathStr;
#endif
}

void SystemData::launchGame(Window* window, FileData* game)
{
	struct Local
	{
		static std::string replaceAll(std::string str, const std::string& replace, const std::string& with)
		{
			size_t pos;
			while ((pos = str.find(replace)) != std::string::npos)
				str = str.replace(pos, replace.length(), with.c_str(), with.length());
			return str;
		}
	};

	LOG(LogInfo) << "Attempting to launch game...";

	AudioManager::getInstance()->deinit();
	VolumeControl::getInstance()->deinit();
#if defined(EXTENSION)
	const std::string controlersConfig = InputManager::getInstance()->configureEmulators();
	LOG(LogInfo) << "Controllers config : " << controlersConfig;
#endif
	window->deinit();

	const std::string rom = escapePath(game->getPath());
	const std::string basename = game->getPath().stem().string();
	const std::string rom_raw = fs::path(game->getPath()).make_preferred().string();

	std::string command = mLaunchCommand;
	command = Local::replaceAll(command, "%ROM%", rom);
	command = Local::replaceAll(command, "%BASENAME%", basename);
	command = Local::replaceAll(command, "%ROM_RAW%", rom_raw);
#if defined(EXTENSION)
	command = Local::replaceAll(command, "%CONTROLLERSCONFIG%", controlersConfig);
	command = Local::replaceAll(command, "%SYSTEM%", game->metadata.get("system"));
	command = Local::replaceAll(command, "%EMULATOR%", game->metadata.get("emulator"));
	command = Local::replaceAll(command, "%CORE%", game->metadata.get("core"));
	command = Local::replaceAll(command, "%RATIO%", game->metadata.get("ratio"));
#endif

	LOG(LogInfo) << "	" << command;
	std::cout << "==============================================\n";
	int exitCode = runSystemCommand(command);
	std::cout << "==============================================\n";

	if (exitCode != 0)
	{
		LOG(LogWarning) << "...launch terminated with nonzero exit code " << exitCode << "!";
	}

	window->init();
	VolumeControl::getInstance()->init();
#if defined(EXTENSION)
	AudioManager::getInstance()->resumeMusic();
#endif
	window->normalizeNextUpdate();

	// update number of times the game has been launched
	int timesPlayed = game->metadata.getInt("playcount") + 1;
	game->metadata.set("playcount", std::to_string(static_cast<long long>(timesPlayed)));

	// update last played time
	boost::posix_time::ptime time = boost::posix_time::second_clock::universal_time();
	game->metadata.setTime("lastplayed", time);
}

void SystemData::populateFolder(FileData* folder)
{
#if defined(EXTENSION)
	FileData::populateRecursiveFolder(folder, mSearchExtensions, this);
#else
	// [...]
#endif
}

std::vector<std::string> readList(const std::string& str, const char* delims = " \t\r\n,")
{
	std::vector<std::string> result;
	size_t prevOff = str.find_first_not_of(delims, 0);
	size_t off = str.find_first_of(delims, prevOff);
	while (off != std::string::npos || prevOff != std::string::npos)
	{
		result.push_back(str.substr(prevOff, off - prevOff));
		prevOff = str.find_first_not_of(delims, off);
		off = str.find_first_of(delims, prevOff);
	}
	return result;
}

struct SystemInfo
{
	std::string name;
	std::string fullname;
	std::string path;
	std::vector<std::string> extensions;
	std::string command;
	std::vector<PlatformIds::PlatformId> platforms;
	std::string theme;
	std::map<std::string, std::vector<std::string>> emulators; // emulators/cores
};

#if defined(EXTENSION) || !defined(EXTENSION)
SystemData* createSystem(pugi::xml_node system)
{
	const std::string name = system.child("name").text().get();
	const std::string fullname = system.child("fullname").text().get();
	const std::string path = [](std::string path) {
		return path.empty() ? path : boost::filesystem::path(path).generic_string(); // convert path to generic directory separators
	}(system.child("path").text().get());

	const std::vector<std::string> extensions = readList(system.child("extension").text().get());

	const std::string cmd = system.child("command").text().get();

	if (name.empty() || path.empty() || extensions.empty() || cmd.empty()) // validate
	{
		LOG(LogError) << "System \"" << name << "\" is missing name, path, extension, or command!";
		return NULL;
	}

	// platform id list
	const char* platformList = system.child("platform").text().get();
	std::vector<PlatformIds::PlatformId> platformIds;
	for (const auto& it : readList(platformList))
	{
		const PlatformIds::PlatformId platformId = PlatformIds::getPlatformId(it.c_str());
		if (platformId == PlatformIds::PLATFORM_IGNORE)
		{
			platformIds.clear(); // Do not allow other platforms
			platformIds.push_back(platformId);
			break;
		}

		// if there appears to be an actual platform ID supplied but it didn't match the list, warn
		if (it.c_str() != NULL && it.c_str()[0] != '\0' && platformId == PlatformIds::PLATFORM_UNKNOWN)
			LOG(LogWarning) << "  Unknown platform for system \"" << name << "\" (platform \"" << it.c_str() << "\" from list \"" << platformList
							<< "\")";
		else if (platformId != PlatformIds::PLATFORM_UNKNOWN)
			platformIds.push_back(platformId);
	}

	// theme folder
	const std::string themeFolder = system.child("theme").text().as_string(name.c_str());

#if defined(EXTENSION)
	// emulators and cores
	std::map<std::string, std::vector<std::string>*>* systemEmulators = new std::map<std::string, std::vector<std::string>*>();
	for (const auto& node : system.child("emulators").children("emulator"))
	{
		const std::string emulatorName = node.attribute("name").as_string();
		(*systemEmulators)[emulatorName] = new std::vector<std::string>();
		for (const auto& coreNode : node.child("cores").children("core"))
			(*systemEmulators)[emulatorName]->push_back(coreNode.text().as_string());
	}

//  pugi::xml_node emulatorsNode = system.child("emulators");
// 	for(pugi::xml_node emuNode = emulatorsNode.child("emulator"); emuNode; emuNode = emuNode.next_sibling("emulator"))
//     {
// 		const std::string emulatorName = emuNode.attribute("name").as_string();
// 		(*systemEmulators)[emulatorName] = new std::vector<std::string>();
// 		pugi::xml_node coresNode = emuNode.child("cores");
// 		for (pugi::xml_node coreNode = coresNode.child("core"); coreNode; coreNode = coreNode.next_sibling("core"))
//         {
// 			(*systemEmulators)[emulatorName]->push_back(coreNode.text().as_string());
// 		}
// 	}
#endif
#if defined(EXTENSION)
	SystemData* newSys = new SystemData(name, fullname, path, extensions, cmd, platformIds, themeFolder, systemEmulators);
#else
	SystemData* newSys = new SystemData(name, fullname, path, extensions, cmd, platformIds, themeFolder);
#endif
	if (newSys->getRootFolder()->getChildren().size() == 0)
	{
		LOG(LogWarning) << "System \"" << name << "\" has no games! Ignoring it.";
		delete newSys;
		return NULL;
	}
	else
	{
		LOG(LogWarning) << "Adding \"" << name << "\" in system list.";
		return newSys;
	}
}
#endif

// creates systems from information located in a config file
bool SystemData::loadConfig()
{
	deleteSystems();

	const std::string path = getConfigPath(false);
	LOG(LogInfo) << "Loading system config file " << path << "...";

	if (!fs::exists(path))
	{
		LOG(LogError) << "es_systems.cfg file does not exist!";
		writeExampleConfig(getConfigPath(true));
		return false;
	}

	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_file(path.c_str());
	if (!result)
	{
		LOG(LogError) << "Could not parse es_systems.cfg file: " << result.description();
		return false;
	}

	// Read the file
	pugi::xml_node systemList = doc.child("systemList");
	if (systemList == nullptr)
	{
		LOG(LogError) << "es_systems.cfg is missing the <systemList> tag!";
		return false;
	}

	// THE CREATION OF EACH SYSTEM
	boost::asio::io_service ioService;
	boost::thread_group threadpool;
	boost::asio::io_service::work work(ioService);
	std::vector<boost::thread*> threads;
	for (int i = 0; i < 4; i++)
		threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));

	std::vector<boost::unique_future<SystemData*>> pending_data;
	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		LOG(LogInfo) << "creating thread for system " << system.child("name").text().get();

		typedef boost::packaged_task<SystemData*> task_t;
		boost::shared_ptr<task_t> task = boost::make_shared<task_t>(boost::bind(&createSystem, system));
		boost::unique_future<SystemData*> fut = task->get_future();
		pending_data.push_back(std::move(fut));
		ioService.post(boost::bind(&task_t::operator(), task));
	}
	boost::wait_for_all(pending_data.begin(), pending_data.end());

	for (auto pending = pending_data.begin(); pending != pending_data.end(); pending++)
	{
		SystemData* result = pending->get();
		if (result != NULL)
			sSystemVector.push_back(result);
	}
	ioService.stop();
	threadpool.join_all();

#if defined(EXTENSION)
	// Favorite system
	for (pugi::xml_node system = systemList.child("system"); system; system = system.next_sibling("system"))
	{
		const std::string name = system.child("name").text().get();
		const std::string fullname = system.child("fullname").text().get();
		const std::string cmd = system.child("command").text().get();
		const std::string themeFolder = system.child("theme").text().as_string(name.c_str());

		if (name == "favorites")
		{
			LOG(LogInfo) << "creating favorite system";
			SystemData* newSys = new SystemData("favorites", fullname, cmd, themeFolder, &sSystemVector);
			sSystemVector.push_back(newSys);
		}
	}
#endif

	return true;
}

void SystemData::deleteSystems()
{
#if defined(EXTENSION)
	if (sSystemVector.size())
	{
		// THE DELETION OF EACH SYSTEM
		boost::asio::io_service ioService;
		boost::thread_group threadpool;
		boost::asio::io_service::work work(ioService);
		std::vector<boost::thread*> threads;
		for (int i = 0; i < 4; i++)
		{
			threadpool.create_thread(boost::bind(&boost::asio::io_service::run, &ioService));
		}

		std::vector<boost::unique_future<bool>> pending_data;
		for (unsigned int i = 0; i < sSystemVector.size(); i++)
		{
			if (!(sSystemVector.at(i))->isFavorite())
			{
				typedef boost::packaged_task<bool> task_t;
				boost::shared_ptr<task_t> task = boost::make_shared<task_t>(boost::bind<bool>(
					[](SystemData* system) {
						delete system;
						return true;
					},
					sSystemVector.at(i)));
				// boost::unique_future<bool> fut = ;
				pending_data.push_back(std::move(task->get_future()));
				ioService.post(boost::bind(&task_t::operator(), task));
			}
		}

		boost::wait_for_all(pending_data.begin(), pending_data.end());

		ioService.stop();
		threadpool.join_all();
		sSystemVector.clear();
	}
#else
	for (auto system : sSystemVector)
	{
		delete system;
	}
	sSystemVector.clear();
#endif
}

std::string SystemData::getConfigPath(bool forWrite)
{
	const fs::path path = getHomePath() + "/.emulationstation/es_systems.cfg";
	return (forWrite || fs::exists(path)) ? path.generic_string() : "/etc/emulationstation/es_systems.cfg";
}

std::string SystemData::getGamelistPath(bool forWrite) const
{
	fs::path filePath = mRootFolder->getPath() / "gamelist.xml";
	if (fs::exists(filePath))
		return filePath.generic_string();

	// If we have a gamelist in the rom directory, we use it
	// else we try to create it
	if (forWrite) // make sure the directory exists if we're going to write to it, or crashes will happen
	{
		if (fs::exists(filePath.parent_path()) || fs::create_directories(filePath.parent_path()))
			return filePath.generic_string();
	}
	// Unable to get or create directory in roms, fallback on ~
	filePath = getHomePath() + "/.emulationstation/gamelists/" + mName + "/gamelist.xml";
	fs::create_directories(filePath.parent_path());
	return filePath.generic_string();
}

std::string SystemData::getThemePath() const
{
	// where we check for themes, in order:
	// 1. [SYSTEM_PATH]/theme.xml
	// 2. currently selected theme set

	// first, check game folder
	const fs::path localThemePath = mRootFolder->getPath() / "theme.xml";
	if (fs::exists(localThemePath))
		return localThemePath.generic_string();

	// not in game folder, try theme sets
	return ThemeData::getThemeFromCurrentSet(mThemeFolder).generic_string();
}

bool SystemData::hasGamelist() const
{
	return (fs::exists(getGamelistPath(false)));
}

unsigned int SystemData::getGameCount() const
{
	return mRootFolder->getFilesRecursive(GAME).size();
}

#if defined(EXTENSION)
unsigned int SystemData::getFavoritesCount() const
{
	return mRootFolder->getFavoritesRecursive(GAME).size();
}

unsigned int SystemData::getHiddenCount() const
{
	return mRootFolder->getHiddenRecursive(GAME).size();
}
#endif

void SystemData::loadTheme()
{
	mTheme = std::make_shared<ThemeData>();

	const std::string path = getThemePath();

	if (!fs::exists(path)) // no theme available for this platform
		return;

	try
	{
		mTheme->loadFile(path);
#if defined(EXTENSION)
		mHasFavorites = mTheme->getHasFavoritesInTheme();
#endif
	}
	catch (ThemeException& e)
	{
		LOG(LogError) << e.what();
		mTheme = std::make_shared<ThemeData>(); // reset to empty
	}
}

#if defined(EXTENSION)
void SystemData::refreshRootFolder()
{
	mRootFolder->clear();
	populateFolder(mRootFolder);
	mRootFolder->sort(FileSorts::SortTypes.at(0));
}

const std::map<std::string, std::vector<std::string>*>* SystemData::getEmulators() const
{
	return mEmulators;
}

SystemData* SystemData::getFavoriteSystem()
{
	for (auto system : sSystemVector)
	{
		if (system->isFavorite())
			return system;
	}
	return NULL;
}
#endif
