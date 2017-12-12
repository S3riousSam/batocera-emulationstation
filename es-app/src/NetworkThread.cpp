#if defined(EXTENSION) // Author: matthieu  2015-02-06
#include "NetworkThread.h"
#include "LocaleES.h"
#include "RecalboxConf.h"
#include "SystemInterface.h"
#include "Window.h"
#include "guis/GuiMsgBox.h"

NetworkThread::NetworkThread(Window& window)
	: mWindow(window)
	, mFirstRun(true)
	, mRunning(true)
	, mThreadHandle(boost::bind(&NetworkThread::run, this))
{
}

NetworkThread::~NetworkThread()
{
	mThreadHandle.join();
}

void NetworkThread::run()
{
	while (mRunning)
	{
		const boost::posix_time::time_duration delay = mFirstRun
			? reinterpret_cast<boost::posix_time::time_duration&>(boost::posix_time::seconds(15))
			: reinterpret_cast<boost::posix_time::time_duration&>(boost::posix_time::hours(1));

		if (mFirstRun)
			mFirstRun = false;

		boost::this_thread::sleep(delay);

		if ((RecalboxConf::get("updates.enabled") == "1") && SystemInterface::canUpdate())
		{
			mWindow.displayMessage(_("AN UPDATE IS AVAILABLE FOR BATOCERA.LINUX"));
			mRunning = false;
		}
	}
}
#endif
