#ifndef _INPUTCONFIG_H_
#define _INPUTCONFIG_H_

#include "pugixml/pugixml.hpp"
#include <SDL.h>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#define DEVICE_KEYBOARD -1
#if defined(EXTENSION)
#define MAX_PLAYERS 5
#endif

enum InputType
{
	TYPE_AXIS,
	TYPE_BUTTON,
	TYPE_HAT,
	TYPE_KEY,
	TYPE_COUNT
};

class Input
{
public:
	int device;
	InputType type;
	int id;
	int value;
	bool configured;

	Input()
		: device(DEVICE_KEYBOARD)
		, type(TYPE_COUNT)
		, id(-01)
		, value(-999)
		, configured(false)
	{
	}

	Input(int dev, InputType t, int i, int val, bool conf)
		: device(dev)
		, type(t)
		, id(i)
		, value(val)
		, configured(conf)
	{
	}

private:
	std::string getHatDir(int val) const
	{
		if (val & SDL_HAT_UP)
			return "up";
		else if (val & SDL_HAT_DOWN)
			return "down";
		else if (val & SDL_HAT_LEFT)
			return "left";
		else if (val & SDL_HAT_RIGHT)
			return "right";
		return "neutral?";
	}

public:
	std::string string() const
	{
		std::stringstream stream;
		switch (type)
		{
		case TYPE_BUTTON:
			stream << "Button " << id;
			break;
		case TYPE_AXIS:
			stream << "Axis " << id << (value > 0 ? "+" : "-");
			break;
		case TYPE_HAT:
			stream << "Hat " << id << " " << getHatDir(value);
			break;
		case TYPE_KEY:
			stream << "Key " << SDL_GetKeyName((SDL_Keycode)id);
			break;
		default:
			stream << "Input to string error";
			break;
		}

		return stream.str();
	}
};

class InputConfig
{
public:
	InputConfig(int deviceId, int deviceIndex, const std::string& deviceName, const std::string& deviceGUID, int deviceNbAxes);

	void clear();
	void mapInput(const std::string& name, Input input);
	void unmapInput(const std::string& name); // unmap all Inputs mapped to this name

	int getDeviceId() const
	{
		return mDeviceId;
	};
	const std::string& getDeviceName()
	{
		return mDeviceName;
	}
	const std::string& getDeviceGUID()
	{
		return mDeviceGUID;
	}
#if defined(EXTENSION)
	int getDeviceIndex() const
	{
		return mDeviceIndex;
	};
	int getDeviceNbAxes() const
	{
		return mDeviceNbAxes;
	};
#endif

	// Returns true if Input is mapped to this name, false otherwise.
	bool isMappedTo(const std::string& name, Input input);

	// Returns the list of names this input is mapped to.
	std::vector<std::string> getMappedTo(Input input);

	void loadFromXML(pugi::xml_node root); // load xml into mNameMap
	void writeToXML(pugi::xml_node parent) const; // write xml from mNameMap

	bool isConfigured() const;

private:
	std::map<std::string, Input> mNameMap;
	const int mDeviceId;
	const std::string mDeviceName;
	const std::string mDeviceGUID;
#if defined(EXTENSION)
	const int mDeviceIndex;
	const int mDeviceNbAxes; // number of axes of the device
#endif
};

#endif
