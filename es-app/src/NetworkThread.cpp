// Author: matthieu  2015-02-06
#include "NetworkThread.h"
#include "LocaleES.h"
#include "RecalboxConf.h"
#include "RecalboxSystem.h"
#include "Window.h"
#include "guis/GuiMsgBox.h"

NetworkThread::NetworkThread(Window* window)
	: mWindow(window)
	, mFirstRun(true)
	, mRunning(true)
{
	mThreadHandle = new boost::thread(boost::bind(&NetworkThread::run, this));
}

NetworkThread::~NetworkThread()
{
	mThreadHandle->join();
}

void NetworkThread::run()
{
	while (mRunning)
	{
		if (mFirstRun)
		{
			boost::this_thread::sleep(boost::posix_time::seconds(15));
			mFirstRun = false;
		}
		else
		{
			boost::this_thread::sleep(boost::posix_time::hours(1));
		}

		if (RecalboxConf::getInstance()->get("updates.enabled") == "1")
		{
			if (RecalboxSystem::getInstance()->canUpdate())
			{
				mWindow->displayMessage(_("AN UPDATE IS AVAILABLE FOR BATOCERA.LINUX"));
				mRunning = false;
			}
		}
	}
}
