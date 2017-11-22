#if defined(EXTENSION) // Created by matthieu on 03/04/16.
#pragma once
#include <map>
#include <string>

class LibretroRatio
{
public:
	std::map<std::string, std::string>* getRatio();
	static LibretroRatio* getInstance();

private:
	static LibretroRatio* sInstance;
	std::map<std::string, std::string>* ratioMap;
	LibretroRatio();
};
#endif
