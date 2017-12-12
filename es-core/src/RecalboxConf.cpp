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
	const char* const CONFIG_FILE_PATH_TMP = "/recalbox/share/system/recalbox.conf.tmp";

	std::map<std::string, std::string> confMap;
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
				confMap[std::string(lineInfo["key"])] = std::string(lineInfo["val"]);
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
		LOG(LogError) << "Unable to open for saving:  " << CONFIG_FILE_PATH << "\n";
		return false;
	}
	// Read all lines in a vector
	std::vector<std::string> fileLines;
	std::string line;
	while (std::getline(filein, line))
		fileLines.push_back(line);
	filein.close();

	// Save new value if exists
	for (std::map<std::string, std::string>::iterator it = confMap.begin(); it != confMap.end(); ++it)
	{
		const std::string key = it->first;
		const std::string val = it->second;
		bool lineFound = false;
		for (size_t i = 0; i < fileLines.size(); i++)
		{
			const std::string currentLine = fileLines[i];

			if (boost::starts_with(currentLine, key + "=") || boost::starts_with(currentLine, ";" + key + "="))
			{
				fileLines[i] = key + "=" + val;
				lineFound = true;
			}
		}
		if (!lineFound)
		{
			fileLines.push_back(key + "=" + val);
		}
	}
	std::ofstream fileout(CONFIG_FILE_PATH_TMP); // Temporary file
	if (!fileout)
	{
		LOG(LogError) << "Unable to open for saving :  " << CONFIG_FILE_PATH_TMP << "\n";
		return false;
	}
	for (size_t i = 0; i < fileLines.size(); i++)
	{
		fileout << fileLines[i] << "\n";
	}

	fileout.close();

	// Copy back the tmp to recalbox.conf
	std::ifstream src(CONFIG_FILE_PATH_TMP, std::ios::binary);
	std::ofstream dst(CONFIG_FILE_PATH, std::ios::binary);
	dst << src.rdbuf();

	remove(CONFIG_FILE_PATH_TMP);

	return true;
}

std::string RecalboxConf::get(const std::string& name, const std::string& defaut)
{
	LoadOnce();
	return confMap.count(name) != 0 ? confMap.at(name) : defaut;
}
void RecalboxConf::set(const std::string& name, const std::string& value)
{
	LoadOnce();
	confMap[name] = value;
}
#endif