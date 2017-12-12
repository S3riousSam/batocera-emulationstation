#if defined(EXTENSION) // Created by matthieu on 12/09/15.
#pragma once
#include <string>

namespace RecalboxConf
{
	bool saveRecalboxConf();
	std::string get(const std::string& name, const std::string& defaut = std::string());
	void set(const std::string& name, const std::string& value);
}; // namespace RecalboxConf
#endif