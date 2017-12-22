#include "platform.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <stdlib.h>
#if defined(WIN32)
#include <codecvt>
#else
#include <sys/statvfs.h>
#endif

namespace
{
	std::string getEnvironmentVariable(const char* const NAME)
	{
#if defined(WIN32)
		size_t requiredSize = 0;
		getenv_s(&requiredSize, nullptr, 0, NAME);
		if (requiredSize == 0)
			return std::string(); // the variable does not exist

		std::vector<char> buffer;
		buffer.resize(requiredSize + 1);

		// Get the value of the LIB environment variable.
		getenv_s(&requiredSize, &buffer[0], requiredSize, NAME);

		return &buffer[0];
#else
		const char* value = getenv(NAME);
		return value != nullptr ? value : std::string();
#endif
	}
}

std::string Platform::getHomePath()
{
	static std::string cacheResult;
	if (!cacheResult.empty())
		return cacheResult;

	// This should give something like "/home/YOUR_USERNAME" on Linux and "C:\Users\YOUR_USERNAME\" on Windows...
	std::string homePath = getEnvironmentVariable("HOME");
	if (homePath.empty())
		homePath = getEnvironmentVariable("HOME_ES");

#if defined(WIN32)
	// ...but does not seem to work for Windows XP or Vista, so try something else
	if (homePath.empty())
	{
		homePath = getEnvironmentVariable("HOMEDRIVE") + getEnvironmentVariable("HOMEPATH");
		boost::replace_all(homePath, "\\", "/"); // in-place replacement.
	}
#endif
	// convert path to generic directory separators
	cacheResult = boost::filesystem::path(homePath).generic_string();
	return cacheResult;
}

#if defined(USEFUL)
int Platform::runShutdownCommand()
{
#if defined(WIN32)
	return system("shutdown -s -t 0");
#else // osx / linux
	return system("poweroff");
#endif
}
#endif

int Platform::runRestartCommand()
{
#if defined(WIN32)
	return system("shutdown -r -t 0");
#else // osx / linux
	return system("reboot");
#endif
}

int Platform::runSystemCommand(const std::string& cmd_utf8)
{
#if defined(WIN32)
	// _wsystem to support non-ASCII paths which requires converting from utf8 to a wstring
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
	const std::wstring wchar_str = converter.from_bytes(cmd_utf8);
	return _wsystem(wchar_str.c_str());
#else
	return system((cmd_utf8 + " 2> /recalbox/share/system/logs/es_launch_stderr.log | head -300 > /recalbox/share/system/logs/es_launch_stdout.log").c_str());
#endif
}