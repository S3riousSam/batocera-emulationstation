#pragma once
#define LOG(level)                    \
	\
if(level > Log::getReportingLevel()); \
	\
else Log()                            \
		.get(level)

#include <iostream>
#include <sstream>
#include <string>

enum LogLevel
{
	LogError,
	LogWarning,
	LogInfo,
	LogDebug
};

class Log
{
public:
	~Log();

	std::ostringstream& get(LogLevel level = LogInfo);

	static LogLevel getReportingLevel();
	static void setReportingLevel(LogLevel level);

	static void flush();
	static void open();
	static void close();

private:
	static std::string getLogPath();
	static FILE* getOutput();

	static FILE* file;
	static LogLevel reportingLevel;
	std::ostringstream os;
	LogLevel messageLevel;
};
