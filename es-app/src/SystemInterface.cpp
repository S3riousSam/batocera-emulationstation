#if defined(EXTENSION) // digitallumberjack/Created on 29 novembre 2014, 03:15
#include "SystemInterface.h"
#if !defined(WIN32)
#include <sys/statvfs.h>
#endif
#include "AudioManager.h"
#include "HttpReq.h"
#include "InputManager.h"
#include "Log.h"
#include "Settings.h"
#include "VolumeControl.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#if !defined(WIN32)
#include <ifaddrs.h>
#include <netinet/in.h>
#endif
#if !defined(WIN32)
#include <arpa/inet.h>
#endif
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>

#if defined(WIN32)
#define popen _popen
#define pclose _pclose
#endif

namespace
{
	const std::string& GetConfigValue(const char* const name)
	{
		return Settings::getInstance()->getString(name);
	}

	const std::string& GetMainScript()
	{
		return GetConfigValue("RecalboxSettingScript");
	}

	bool halt(bool reboot, bool fast)
	{
		SDL_Event quit;
		quit.type = (fast ? SystemInterface::SDL_FAST_QUIT : SDL_QUIT) | (reboot ? SystemInterface::SDL_RB_REBOOT : SystemInterface::SDL_RB_SHUTDOWN);
		SDL_PushEvent(&quit);
		return true;
	}

	bool ExecuteCommand(const char* const label, const std::string& command)
	{
		LOG(LogInfo) << "Executing Command: " << command;
		if (system(command.c_str()))
		{
			LOG(LogWarning) << "Error executing " << command;
			return false;
		}
		else
		{
			LOG(LogInfo) << label;
			return true;
		}
	}

	template<typename T>
	bool ExecuteCommand(const char* const label, const std::string& command, const T& value)
	{
		LOG(LogInfo) << "Executing Command: " << command;
		if (system(command.c_str()))
		{
			LOG(LogWarning) << "Error executing " << command;
			return false;
		}
		else
		{
			LOG(LogInfo) << label << " set to: " << value;
			return true;
		}
	}
} // namespace

unsigned long SystemInterface::getFreeSpaceGB(std::string mountpoint)
{
#if !defined(WIN32)
	struct statvfs fiData;
	const char* fnPath = mountpoint.c_str();
	int free = 0;
	if ((statvfs(fnPath, &fiData)) >= 0)
	{
		free = (fiData.f_bfree * fiData.f_bsize) / (1024 * 1024 * 1024);
	}
	return free;
#else
	return 0;
#endif
}

std::string SystemInterface::getFreeSpaceInfo()
{
	std::string sharePart = GetConfigValue("SharePartition");
	if (!sharePart.empty())
	{
		const char* fnPath = sharePart.c_str();
#if !defined(WIN32)
		struct statvfs fiData;
		if ((statvfs(fnPath, &fiData)) < 0)
		{
			return std::string();
		}
		else
		{
			const unsigned long total = (fiData.f_blocks * (fiData.f_bsize / 1024)) / (1024L * 1024L);
			const unsigned long free = (fiData.f_bfree * (fiData.f_bsize / 1024)) / (1024L * 1024L);
			const unsigned long used = (total - free);
			std::ostringstream oss;
			if (total != 0)
			{ // for small SD card ;) with share < 1GB
				const unsigned long percent = used * 100 / total;
				oss << used << "GB/" << total << "GB (" << percent << "%)";
			}
			else
				oss << "N/A";
			return oss.str();
		}
#else
		(void)(fnPath);
		return "TODO";
#endif
	}
	else
	{
		return "ERROR";
	}
}

bool SystemInterface::isFreeSpaceLimit()
{
	const std::string sharePart = GetConfigValue("SharePartition");
	return !sharePart.empty() ? getFreeSpaceGB(sharePart) < 2 : "ERROR"; // bool?
}

std::string SystemInterface::getVersion()
{
	const std::string version = GetConfigValue("VersionFile");
	if (!version.empty())
	{
		std::ifstream ifs(version);
		if (ifs.good())
		{
			std::string contents;
			std::getline(ifs, contents);
			return contents;
		}
	}
	return std::string();
}

#if defined(USEFUL)
bool SystemInterface::needToShowVersionMessage()
{
	createLastVersionFileIfNotExisting();
	const std::string& versionFile = GetConfigValue("LastVersionFile");
	if (versionFile.size() > 0)
	{
		std::ifstream lvifs(versionFile);
		if (lvifs.good())
		{
			std::string lastVersion;
			std::getline(lvifs, lastVersion);
			std::string currentVersion = getVersion();
			if (lastVersion == currentVersion)
				return false;
		}
	}
	return true;
}
#endif

bool SystemInterface::createLastVersionFileIfNotExisting()
{
	const std::string& versionFile = GetConfigValue("LastVersionFile");
	FILE* file = fopen(versionFile.c_str(), "r");
	if (file != NULL)
	{
		fclose(file);
		return true;
	}

	return updateLastVersionFile();
}

bool SystemInterface::updateLastVersionFile()
{
	const std::string& versionFile = GetConfigValue("LastVersionFile");
	const std::string command = std::string("echo ") + getVersion() + " > " + versionFile;
	return ExecuteCommand("Last version file updated", command);
}

std::string SystemInterface::getVersionMessage()
{
	const std::string& versionMessageFile = GetConfigValue("VersionMessage");
	if (versionMessageFile.empty())
		return std::string();

	std::ifstream ifs(versionMessageFile);
	return ifs.good() ? std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>()) : std::string();
}

bool SystemInterface::setOverscan(bool enable)
{
	const std::string command = GetMainScript() + (enable ? " overscan enable" : " overscan disable");
	return ExecuteCommand("Overscan", command, enable);
}

bool SystemInterface::setOverclock(const std::string& mode)
{
	if (mode.empty())
		return false;
	const std::string command = GetMainScript() + " overclock " + mode;
	return ExecuteCommand("Overclocking", command, mode);
}

std::pair<std::string, int> executeCommand(
	BusyComponent& ui, const std::string& commandLine, std::function<bool(const std::string&)> filter = std::function<bool(const std::string&)>())
{
	FILE* pipe = popen(commandLine.c_str(), "r");
	char line[1024] = "";
	if (pipe == NULL)
	{
		return std::pair<std::string, int>(std::string("Cannot call update command"), -1);
	}

	// Write the command output into the logs.
	FILE* flog = fopen("/recalbox/share/system/logs/recalbox-upgrade.log", "w");
	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		if (flog != NULL)
			fprintf(flog, "%s\n", line);

		if (!filter || filter(line))
			ui.setText(std::string(line));
	}
	if (flog != NULL)
		fclose(flog);

	const int exitCode = pclose(pipe);
	return std::pair<std::string, int>(std::string(line), exitCode);
}

std::pair<std::string, int> SystemInterface::updateSystem(BusyComponent* ui)
{
	return executeCommand(*ui, GetConfigValue("UpdateCommand"));
}

std::pair<std::string, int> SystemInterface::backupSystem(BusyComponent* ui, const std::string& device)
{
	return executeCommand(*ui, std::string("/recalbox/scripts/recalbox-sync.sh sync ") + device);
}

std::pair<std::string, int> SystemInterface::installSystem(BusyComponent* ui, const std::string& device, const std::string& architecture)
{
	return executeCommand(*ui, std::string("/recalbox/scripts/recalbox-sync.sh sync ") + device + " " + architecture);
}

std::pair<std::string, int> SystemInterface::scrape(BusyComponent* ui)
{
	return executeCommand(*ui, "/recalbox/scripts/recalbox-scraper.sh", [](const std::string& line) { return boost::starts_with(line, "GAME: "); });
}

bool SystemInterface::ping()
{
	return system(std::string("ping -c 1 " + GetConfigValue("UpdateServer")).c_str()) == 0;
}

bool SystemInterface::canUpdate()
{
	const std::string command = GetMainScript() + " canupdate";
	LOG(LogInfo) << "Launching " << command;
	if (system(command.c_str()) == 0)
	{
		LOG(LogInfo) << "Can update ";
		return true;
	}
	else
	{
		LOG(LogInfo) << "Cannot update ";
		return false;
	}
}

bool SystemInterface::launchKodi(Window* window)
{
	LOG(LogInfo) << "Attempting to launch Kodi...";

	AudioManager::getInstance()->deinit();
	VolumeControl::getInstance()->deinit();

	const std::string command = "configgen -system kodi -rom '' " + InputManager::getInstance()->configureEmulators();

	window->deinit();

	const int exitCode = system(command.c_str());
#if !defined(WIN32)
	// WIFEXITED returns a nonzero value if the child process terminated normally with exit or _exit.
	// https://www.gnu.org/software/libc/manual/html_node/Process-Completion-Status.html
	if (WIFEXITED(exitCode))
	{
		exitCode = WEXITSTATUS(exitCode);
	}
#endif

	window->init();
	VolumeControl::getInstance()->init();
	AudioManager::getInstance()->resumeMusic();

	window->normalizeNextUpdate();

	// handle end of Kodi
	switch (exitCode)
	{
	case 10: // reboot code
		reboot();
		return true;
		break;
	case 11: // shutdown code
		shutdown();
		return true;
		break;
	}

	return (exitCode == 0);
}

bool SystemInterface::launchFileManager(Window* window)
{
	LOG(LogInfo) << "Attempting to launch file manager...";

	AudioManager::getInstance()->deinit();
	VolumeControl::getInstance()->deinit();

	window->deinit();

	const int exitCode = system("/recalbox/scripts/filemanagerlauncher.sh");
#if !defined(WIN32)
	if (WIFEXITED(exitCode))
	{
		exitCode = WEXITSTATUS(exitCode);
	}
#endif
	window->init();
	VolumeControl::getInstance()->init();
	AudioManager::getInstance()->resumeMusic();

	window->normalizeNextUpdate();

	return (exitCode == 0);
}

bool SystemInterface::enableWifi(std::string ssid, std::string key)
{
	std::ostringstream oss;
	boost::replace_all(ssid, "\"", "\\\"");
	boost::replace_all(key, "\"", "\\\"");
	oss << GetMainScript() << " wifi enable \"" << ssid << "\" \"" << key << "\"";
	const std::string command = oss.str();
	LOG(LogInfo) << "Launching " << command;
	if (system(command.c_str()) == 0)
	{
		LOG(LogInfo) << "Enabling Wifi succeeded";
		return true;
	}
	else
	{
		LOG(LogInfo) << "Enabling Wifi failed";
		return false;
	}
}

bool SystemInterface::disableWifi()
{
	std::ostringstream oss;
	oss << GetMainScript() << " wifi disable";
	const std::string command = oss.str();
	LOG(LogInfo) << "Launching " << command;
	const bool result = (system(command.c_str()) == 0);
	LOG(LogInfo) << "Disabling Wifi " << (result ? "succeeded" : "failed");
	return result;
}

bool SystemInterface::reboot()
{
	return halt(true, false);
}
bool SystemInterface::fastReboot()
{
	return halt(true, true);
}
bool SystemInterface::shutdown()
{
	return halt(false, false);
}
bool SystemInterface::fastShutdown()
{
	return halt(false, true);
}

std::string SystemInterface::getIpAdress()
{
#if !defined(WIN32)
	struct ifaddrs* ifAddrStruct = NULL;
	struct ifaddrs* ifa = NULL;
	void* tmpAddrPtr = NULL;

	std::string result = "NOT CONNECTED";
	getifaddrs(&ifAddrStruct);

	for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (!ifa->ifa_addr)
		{
			continue;
		}
		if (ifa->ifa_addr->sa_family == AF_INET)
		{ // check it is IP4
			// is a valid IP4 Address
			tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
			char addressBuffer[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
			printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
			if (std::string(ifa->ifa_name).find("eth") != std::string::npos || std::string(ifa->ifa_name).find("wlan") != std::string::npos)
			{
				result = std::string(addressBuffer);
			}
		}
	}
	// Seeking for ipv6 if no IPV4
	if (result.compare("NOT CONNECTED") == 0)
	{
		for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
		{
			if (!ifa->ifa_addr)
			{
				continue;
			}
			if (ifa->ifa_addr->sa_family == AF_INET6)
			{ // check it is IP6
				// is a valid IP6 Address
				tmpAddrPtr = &((struct sockaddr_in6*)ifa->ifa_addr)->sin6_addr;
				char addressBuffer[INET6_ADDRSTRLEN];
				inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
				printf("%s IP Address %s\n", ifa->ifa_name, addressBuffer);
				if (std::string(ifa->ifa_name).find("eth") != std::string::npos || std::string(ifa->ifa_name).find("wlan") != std::string::npos)
				{
					return std::string(addressBuffer);
				}
			}
		}
	}
	if (ifAddrStruct != NULL)
		freeifaddrs(ifAddrStruct);
	return result;
#else
	return std::string();
#endif
}

std::vector<std::string>* SystemInterface::scanBluetooth()
{
	std::ostringstream oss;
	oss << GetMainScript() << " hcitoolscan";
	FILE* pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return NULL;

	std::vector<std::string>* result = new std::vector<std::string>(); // Memory leak!
	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		result->push_back(std::string(line));
	}
	pclose(pipe);

	return result;
}

bool SystemInterface::pairBluetooth(std::string& controller)
{
	std::ostringstream oss;
	oss << GetMainScript() << " hiddpair " << controller;
	return system(oss.str().c_str()) == 0;
}

std::vector<std::string> SystemInterface::getAvailableInstallDevices()
{
	std::vector<std::string> res;
	std::ostringstream oss;
	oss << "/recalbox/scripts/recalbox-install.sh listDisks";
	FILE* pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
	{
		return res;
	}

	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		res.push_back(std::string(line));
	}
	pclose(pipe);
	return res;
}

std::vector<std::string> SystemInterface::getAvailableInstallArchitectures()
{
	std::vector<std::string> res;
	std::ostringstream oss;
	oss << "/recalbox/scripts/recalbox-install.sh listArchs";
	FILE* pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
	{
		return res;
	}

	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		res.push_back(std::string(line));
	}
	pclose(pipe);
	return res;
}

std::vector<std::string> SystemInterface::getAvailableStorageDevices()
{
	std::vector<std::string> res;
	std::ostringstream oss;
	oss << GetMainScript() << " storage list";
	FILE* pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return res;

	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		res.push_back(std::string(line));
	}
	pclose(pipe);
	return res;
}

std::vector<std::string> SystemInterface::getAvailableBackupDevices()
{
	std::vector<std::string> res;
	std::ostringstream oss;
	oss << "/recalbox/scripts/recalbox-sync.sh list";
	FILE* pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return res;

	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		res.push_back(std::string(line));
	}
	pclose(pipe);
	return res;
}

std::vector<std::string> SystemInterface::getSystemInformations()
{
	std::vector<std::string> res;
	FILE* pipe = popen("/recalbox/scripts/recalbox-info.sh", "r");
	char line[1024];

	if (pipe == NULL)
		return res;

	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		res.push_back(std::string(line));
	}
	pclose(pipe);
	return res;
}

std::vector<SystemInterface::BiosSystem> SystemInterface::getBiosInformations()
{
	std::vector<BiosSystem> res;
	BiosSystem current;
	bool isCurrent = false;

	FILE* pipe = popen("/recalbox/scripts/recalbox-systems.py", "r");
	char line[1024];

	if (pipe == NULL)
		return res;

	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		if (boost::starts_with(line, "> "))
		{
			if (isCurrent)
			{
				res.push_back(current);
			}
			isCurrent = true;
			current.name = std::string(std::string(line).substr(2));
			current.bios.clear();
		}
		else
		{
			std::vector<std::string> tokens;
			boost::split(tokens, line, boost::is_any_of(" "));
			if (tokens.size() >= 3)
			{
				BiosSystem::BiosFile biosFile = {
					tokens.at(0), // status
					tokens.at(1), // md5
				};

				// concatenate the ending words
				for (size_t i = 2; i < tokens.size(); i++)
				{
					if (i > 2)
						biosFile.path += " ";
					biosFile.path += tokens.at(i);
				}

				current.bios.push_back(biosFile);
			}
		}
	}
	if (isCurrent)
		res.push_back(current);

	pclose(pipe);
	return res;
}

bool SystemInterface::generateSupportFile()
{
#if !defined(WIN32)
	std::string cmd = "/recalbox/scripts/recalbox-support.sh";
	int exitcode = system(cmd.c_str());
	if (WIFEXITED(exitcode))
	{
		exitcode = WEXITSTATUS(exitcode);
	}
	return exitcode == 0;
#else
	return false;
#endif
}

std::string SystemInterface::getCurrentStorage()
{
	std::ostringstream oss;
	oss << GetMainScript() << " "
		<< "storage current";
	FILE* pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return std::string();

	if (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}
	return "INTERNAL";
}

bool SystemInterface::setStorage(std::string selected)
{
	std::ostringstream oss;
	oss << GetMainScript() << " "
		<< "storage"
		<< " " << selected;
	return (system(oss.str().c_str()) == 0);
}

bool SystemInterface::forgetBluetoothControllers()
{
	std::ostringstream oss;
	oss << GetMainScript() << " "
		<< "forgetBT";
	return (system(oss.str().c_str()) == 0);
}

std::string SystemInterface::getRootPassword()
{
	std::ostringstream oss;
	oss << GetMainScript() << " "
		<< "getRootPassword";
	FILE* pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return std::string();

	if (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}
	return oss.str().c_str();
}

std::vector<std::string> SystemInterface::getAvailableAudioOutputDevices()
{
	std::vector<std::string> result;
	std::ostringstream oss;
	oss << GetMainScript() << " "
		<< "lsaudio";
	FILE* pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return result;

	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		result.push_back(std::string(line));
	}
	pclose(pipe);
	return result;
}

std::vector<std::string> SystemInterface::getAvailableVideoOutputDevices()
{
	const std::string cmd = GetMainScript() + " lsoutputs";
	FILE* pipe = popen(cmd.c_str(), "r");
	char line[1024];

	std::vector<std::string> result;
	if (pipe == NULL)
		return result;

	while (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		result.push_back(std::string(line));
	}
	pclose(pipe);
	return result;
}

std::string SystemInterface::getCurrentAudioOutputDevice()
{
	std::ostringstream oss;
	oss << GetMainScript() << " "
		<< "getaudio";
	FILE* pipe = popen(oss.str().c_str(), "r");
	char line[1024];

	if (pipe == NULL)
		return "";

	if (fgets(line, 1024, pipe))
	{
		strtok(line, "\n");
		pclose(pipe);
		return std::string(line);
	}
	return "INTERNAL";
}

bool SystemInterface::setAudioOutputDevice(std::string selected)
{
	AudioManager::getInstance()->deinit();
	VolumeControl::getInstance()->deinit();

	std::ostringstream oss;
	oss << GetConfigValue("RecalboxSettingScript") << " "
		<< "audio"
		<< " '" << selected << "'";
	const int exitcode = system(oss.str().c_str());

	VolumeControl::getInstance()->init();

	AudioManager::getInstance()->resumeMusic();
	AudioManager::getInstance()->playCheckSound();

	return (exitcode == 0);
}
#endif
