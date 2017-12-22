#include "InputConfig.h"
#include "InputManager.h"
#include "Log.h"
#include <SDL.h>
#include <algorithm>
#include <iostream>
#include <string>

namespace
{
	std::string inputTypeToString(InputType type)
	{
		switch (type)
		{
		case TYPE_AXIS:
			return "axis";
		case TYPE_BUTTON:
			return "button";
		case TYPE_HAT:
			return "hat";
		case TYPE_KEY:
			return "key";
		default:
			return "error";
		}
	}

	InputType stringToInputType(const std::string& type)
	{
		if (type == "axis")
			return TYPE_AXIS;
		if (type == "button")
			return TYPE_BUTTON;
		if (type == "hat")
			return TYPE_HAT;
		if (type == "key")
			return TYPE_KEY;
		return TYPE_COUNT;
	}

	std::string toLower(std::string str)
	{
		for (unsigned int i = 0; i < str.length(); i++)
			str[i] = static_cast<char>(tolower(str[i]));
		return str;
	}
}

InputConfig::InputConfig(int deviceId, int deviceIndex, const std::string& deviceName, const std::string& deviceGUID, int deviceNbAxes)
	: mDeviceId(deviceId)
	, mDeviceName(deviceName)
	, mDeviceGUID(deviceGUID)
#if defined(EXTENSION)
	, mDeviceIndex(deviceIndex)
	, mDeviceNbAxes(deviceNbAxes)
#endif
{
}

void InputConfig::clear()
{
	mNameMap.clear();
}

bool InputConfig::isConfigured() const
{
	return mNameMap.size() > 0;
}

void InputConfig::mapInput(const std::string& name, Input input)
{
	mNameMap[toLower(name)] = input;
}

void InputConfig::unmapInput(const std::string& name)
{
	auto it = mNameMap.find(toLower(name));
	if (it != mNameMap.end())
		mNameMap.erase(it);
}

bool InputConfig::isMappedTo(const std::string& name, Input input)
{
	struct Local
	{
		// Returns true if there is an Input mapped to this name, false otherwise.
		// Writes Input mapped to this name to result if true.
		static bool getInputByName(const std::map<std::string, Input>& map, const std::string& name, Input& result)
		{
			const auto it = map.find(toLower(name));
			if (it != map.end())
			{
				result = it->second;
				return true;
			}

			return false;
		}
	};

	Input comp;
	if (!Local::getInputByName(mNameMap, name, comp))
		return false;

	if (comp.configured && comp.type == input.type && comp.id == input.id)
	{
		if (comp.type == TYPE_HAT)
			return (input.value == 0 || input.value & comp.value);
		if (comp.type == TYPE_AXIS)
			return input.value == 0 || comp.value == input.value;
		else
			return true;
	}
	return false;
}

std::vector<std::string> InputConfig::getMappedTo(Input input)
{
	std::vector<std::string> result;

	for (const auto& iterator : mNameMap)
	{
		if (!iterator.second.configured)
			continue;

		if (iterator.second.device == input.device && iterator.second.type == input.type && iterator.second.id == input.id)
		{
			if (iterator.second.type == TYPE_HAT)
			{
				if (input.value == 0 || input.value & iterator.second.value)
					result.push_back(iterator.first);
				continue;
			}

			if (input.type == TYPE_AXIS)
			{
				if (input.value == 0 || iterator.second.value == input.value)
					result.push_back(iterator.first);
			}
			else
			{
				result.push_back(iterator.first);
			}
		}
	}

	return result;
}

void InputConfig::loadFromXML(pugi::xml_node node)
{
	clear();

	for (pugi::xml_node input = node.child("input"); input; input = input.next_sibling("input"))
	{
		const std::string name = input.attribute("name").as_string();
		const std::string type = input.attribute("type").as_string();
		const InputType typeEnum = stringToInputType(type);

		if (typeEnum == TYPE_COUNT)
		{
			LOG(LogError) << "InputConfig load error - input of type \"" << type << "\" is invalid! Skipping input \"" << name << "\".\n";
			continue;
		}

		const int id = input.attribute("id").as_int();
		const int value = input.attribute("value").as_int();

		if (value == 0)
			LOG(LogWarning) << "WARNING: InputConfig value is 0 for " << type << " " << id << "!\n";

		mNameMap[toLower(name)] = Input(mDeviceId, typeEnum, id, value, true);
	}
}

void InputConfig::writeToXML(pugi::xml_node parent) const
{
	pugi::xml_node cfg = parent.append_child("inputConfig");

	if (mDeviceId == DEVICE_KEYBOARD)
	{
		cfg.append_attribute("type") = "keyboard";
		cfg.append_attribute("deviceName") = "Keyboard";
	}
	else
	{
		cfg.append_attribute("type") = "joystick";
		cfg.append_attribute("deviceName") = mDeviceName.c_str();
	}

	cfg.append_attribute("deviceGUID") = mDeviceGUID.c_str();

	for (const auto& iterator : mNameMap)
	{
		if (!iterator.second.configured)
			continue;

		pugi::xml_node input = cfg.append_child("input");
		input.append_attribute("name") = iterator.first.c_str();
		input.append_attribute("type") = inputTypeToString(iterator.second.type).c_str();
		input.append_attribute("id").set_value(iterator.second.id);
		input.append_attribute("value").set_value(iterator.second.value);
	}
}
