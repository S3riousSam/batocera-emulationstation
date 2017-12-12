#if defined(EXTENSION)
#pragma once
#include "Window.h"
#include "components/BusyComponent.h"
#include <string>

namespace SystemInterface
{
	struct BiosSystem
	{
		struct BiosFile
		{
			std::string status;
			std::string md5;
			std::string path;
		};

		std::string name;
		std::vector<BiosFile> bios;
	};

	static const Uint32 SDL_FAST_QUIT = 0x800F;
	static const Uint32 SDL_RB_SHUTDOWN = 0X4000;
	static const Uint32 SDL_RB_REBOOT = 0x2000;

	unsigned long getFreeSpaceGB(std::string mountpoint);
	std::string getFreeSpaceInfo();
	bool isFreeSpaceLimit();

	std::string getVersion();
	std::string getRootPassword();

	bool setOverscan(bool enable);
	bool setOverclock(const std::string& mode);

	bool createLastVersionFileIfNotExisting();

	bool updateLastVersionFile();
#if defined(USEFUL)
	bool needToShowVersionMessage();
#endif
	std::string getVersionMessage();

	std::pair<std::string, int> updateSystem(BusyComponent* ui);
	std::pair<std::string, int> backupSystem(BusyComponent* ui, const std::string& device);
	std::pair<std::string, int> installSystem(BusyComponent* ui, const std::string& device, const std::string& architecture);
	std::pair<std::string, int> scrape(BusyComponent* ui);

	bool ping();

	bool canUpdate();

	bool launchKodi(Window* window);
	bool launchFileManager(Window* window);

	bool enableWifi(std::string ssid, std::string key); // Non-reference intended
	bool disableWifi();

	bool reboot();
	bool shutdown();
	bool fastReboot();
	bool fastShutdown();

	std::string getIpAdress();

	std::vector<std::string>* scanBluetooth();
	bool pairBluetooth(std::string& basic_string);

	std::vector<std::string> getAvailableInstallDevices();
	std::vector<std::string> getAvailableInstallArchitectures();

	std::vector<std::string> getAvailableStorageDevices();
	std::vector<std::string> getAvailableBackupDevices();
	std::vector<std::string> getSystemInformations();
	std::vector<BiosSystem> getBiosInformations();
	bool generateSupportFile();

	std::string getCurrentStorage();

	bool setStorage(std::string basic_string);

	bool forgetBluetoothControllers();

	// audio card
	bool setAudioOutputDevice(std::string device);
	std::vector<std::string> getAvailableAudioOutputDevices();
	std::string getCurrentAudioOutputDevice();

	// video output
	std::vector<std::string> getAvailableVideoOutputDevices();
}; // namespace SystemInterface

#endif