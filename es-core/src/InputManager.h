#pragma once
#include <SDL.h>
#include <map>
#include <string>
#include <vector>

class InputConfig;
class Window;

class InputManager // Singleton
{
private:
	InputManager();

	static InputManager* mInstance;
#if defined(USEFUL)
	void loadDefaultKBConfig();
#endif

	std::map<SDL_JoystickID, SDL_Joystick*> mJoysticks;
	std::map<SDL_JoystickID, InputConfig*> mInputConfigs;
	InputConfig* mKeyboardInputConfig;

	std::map<SDL_JoystickID, int*> mPrevAxisValues;

	bool initialized() const;

	void addJoystickByDeviceIndex(int id);
	void removeJoystickByJoystickID(SDL_JoystickID id);
	bool loadInputConfig(InputConfig& config); // returns true if successfully loaded, false if not (or didn't exist)
#if defined(EXTENSION) || !defined(EXTENSION)
	void clearJoystick();
	void addAllJoysticks();
#endif

public:
	virtual ~InputManager();

	static InputManager* getInstance();

	void writeDeviceConfig(InputConfig* config);
	static std::string getConfigPath();

	void init();
	void deinit();

	int getNumJoysticks();
#if defined(USEFUL)
	int getButtonCountByDevice(int deviceId);
#endif
#if defined(EXTENSION)
	int getAxisCountByDevice(int deviceId);
#endif

	int getNumConfiguredDevices() const;
#if defined(EXTENSION)
	std::string getDeviceGUIDString(int deviceId);
	InputConfig* getInputConfigByDevice(int deviceId);
#endif
	bool parseEvent(const SDL_Event& ev, Window* window);
#if defined(EXTENSION)
	std::string configureEmulators();
#endif
};
