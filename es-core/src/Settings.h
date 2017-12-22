#pragma once
#include <map>
#include <string>

class Settings // Singleton storing settings (es_settings.cfg)
{
public:
	static Settings* getInstance();
private:
	void loadFile();
public:
	void saveFile();

	// You get a warning if you try a get on a key that is not already present.
	bool getBool(const std::string& name);
	int getInt(const std::string& name);
	float getFloat(const std::string& name);
	const std::string& getString(const std::string& name);

	void setBool(const std::string& name, bool value);
	void setInt(const std::string& name, int value);
	void setFloat(const std::string& name, float value);
	void setString(const std::string& name, const std::string& value);

private:
	Settings();

	static Settings* sInstance;
	std::map<std::string, bool> mBoolMap;
	std::map<std::string, int> mIntMap;
	std::map<std::string, float> mFloatMap;
	std::map<std::string, std::string> mStringMap;
};
