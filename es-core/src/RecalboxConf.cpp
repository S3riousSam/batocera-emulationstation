#if defined(EXTENSION)
#include "RecalboxConf.h"
#include "Log.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/regex.hpp>
#include <fstream>
#include <iostream>

namespace
{
	const char* const CONFIG_FILE_PATH = "/recalbox/share/system/recalbox.conf";
	std::map<std::string, std::string> mapConfig;
	bool loadedOnce = false;
} // namespace

void LoadOnce()
{
	if (loadedOnce)
		return;

	std::string line;
	std::ifstream recalboxConf(CONFIG_FILE_PATH);
	if (recalboxConf && recalboxConf.is_open())
	{
		while (std::getline(recalboxConf, line))
		{
			boost::smatch lineInfo;
			if (boost::regex_match(line, lineInfo, boost::regex("^(?<key>[^;|#].*?)=(?<val>.*?)$")))
			{
				mapConfig[std::string(lineInfo["key"])] = std::string(lineInfo["val"]);
			}
		}
		recalboxConf.close();
	}
	else
	{
		LOG(LogError) << "Unable to open " << CONFIG_FILE_PATH;
	}
	loadedOnce = true;
}

bool RecalboxConf::saveRecalboxConf()
{
	LoadOnce();

	std::ifstream filein(CONFIG_FILE_PATH); // File to read from
	if (!filein)
	{
		LOG(LogError) << "Unable to open for saving: " << CONFIG_FILE_PATH;
		return false;
	}
	// Read all lines in a vector
	std::vector<std::string> fileLines;
	{
		std::string line;
		while (std::getline(filein, line))
			fileLines.push_back(line);
		filein.close();
	}

	// Save new value if exists
	for (const auto& itParam : mapConfig)
	{
		const char* const EQUAL_SPLITTER = "=";
		const std::string& key = itParam.first;
		const std::string& val = itParam.second;
		bool lineFound = false;
		for (auto& line : fileLines)
		{
			const std::string currentLine = line;
			if (boost::starts_with(currentLine, key + EQUAL_SPLITTER) ||
				boost::starts_with(currentLine, ";" + key + EQUAL_SPLITTER))
			{
				line = key + EQUAL_SPLITTER + val; // update value
				lineFound = true;
			}
		}
		if (!lineFound)
			fileLines.push_back(key + EQUAL_SPLITTER + val); // new value
	}

	const std::string configFilePathTemp = std::string(CONFIG_FILE_PATH) + ".tmp";
	std::ofstream fileout(configFilePathTemp.c_str()); // Temporary file
	if (!fileout)
	{
		LOG(LogError) << "Unable to open for saving: " << configFilePathTemp;
		return false;
	}
	for (auto& line : fileLines)
		fileout << line << "\n";
	fileout.close();

	// Copy back the tmp to recalbox.conf
	const std::ifstream src(configFilePathTemp.c_str(), std::ios::binary);
	std::ofstream dst(CONFIG_FILE_PATH, std::ios::binary);
	dst << src.rdbuf();

	remove(configFilePathTemp.c_str());
	return true;
}

std::string RecalboxConf::get(const std::string& name, const std::string& defaut)
{
	LoadOnce();
	return mapConfig.count(name) != 0 ? mapConfig.at(name) : defaut;
}
void RecalboxConf::set(const std::string& name, const std::string& value)
{
	LoadOnce();
	mapConfig[name] = value;
}
#endif