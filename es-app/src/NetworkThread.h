#if defined(EXTENSION)
#pragma once
#include <boost/thread.hpp>

class Window;

class NetworkThread
{
public:
	NetworkThread(Window& window);
	virtual ~NetworkThread();

private:
	Window& mWindow;
	bool mFirstRun;
	bool mRunning;
	boost::thread mThreadHandle;
	void run();
};
#endif