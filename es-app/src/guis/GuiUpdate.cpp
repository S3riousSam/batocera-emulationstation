#if defined(EXTENSION)
#include "guis/GuiUpdate.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"
#include "Log.h"
#include "Settings.h"
#include "SystemInterface.h"
#include "Window.h"
#include <boost/thread.hpp>
#include <string>

GuiUpdate::GuiUpdate(Window* window)
	: GuiComponent(window)
	, mBusyAnim(window)
{
	setSize(Renderer::getScreenSize());
	mLoading = true;
	mPingHandle = new boost::thread(boost::bind(&GuiUpdate::threadPing, this));
	mBusyAnim.setSize(mSize);
}

GuiUpdate::~GuiUpdate()
{
	mPingHandle->join();
}

bool GuiUpdate::input(InputConfig* config, Input input)
{
	return false;
}

std::vector<HelpPrompt> GuiUpdate::getHelpPrompts()
{
	return std::vector<HelpPrompt>();
}

void GuiUpdate::render(const Eigen::Affine3f& parentTrans)
{
	Eigen::Affine3f trans = parentTrans * getTransform();

	renderChildren(trans);

	Renderer::setMatrix(trans);
	Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

	if (mLoading)
		mBusyAnim.render(trans);
}

void GuiUpdate::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	mBusyAnim.update(deltaTime);

	Window* window = mWindow;
	if (mState == 1)
	{
		window->pushGui(new GuiMsgBox(window, _("REALLY UPDATE?"), _("YES"),
			[this] {
				mState = 2;
				mLoading = true;
				mHandle = new boost::thread(boost::bind(&GuiUpdate::threadUpdate, this));
			},
			_("NO"), [this] { mState = -1; }));
		mState = 0;
	}

	if (mState == 3)
	{
		window->pushGui(new GuiMsgBox(window, _("NETWORK CONNECTION NEEDED"), _("OK"), [this] { mState = -1; }));
		mState = 0;
	}
	if (mState == 4)
	{
		window->pushGui(new GuiMsgBox(window, _("UPDATE DOWNLOADED, THE SYSTEM WILL NOW REBOOT"), _("OK"), [this] {
			if (runRestartCommand() != 0)
			{
				LOG(LogWarning) << "Reboot terminated with non-zero result!";
			}
		}));
		mState = 0;
	}
	if (mState == 5)
	{
		window->pushGui(new GuiMsgBox(window, mResult.first, _("OK"), [this] { mState = -1; }));
		mState = 0;
	}
	if (mState == 6)
	{
		window->pushGui(new GuiMsgBox(window, _("NO UPDATE AVAILABLE"), _("OK"), [this] { mState = -1; }));
		mState = 0;
	}
	if (mState == -1)
	{
		delete this;
	}
}

void GuiUpdate::threadUpdate()
{
	std::pair<std::string, int> updateStatus = SystemInterface::updateSystem(&mBusyAnim);
	if (updateStatus.second == 0)
		onUpdateOk();
	else
		onUpdateError(updateStatus);
}

void GuiUpdate::threadPing()
{
	if (SystemInterface::ping())
	{
		if (SystemInterface::canUpdate())
			onUpdateAvailable();
		else
			onNoUpdateAvailable();
	}
	else
	{
		onPingError();
	}
}
void GuiUpdate::onUpdateAvailable()
{
	mLoading = false;
	LOG(LogInfo) << "update available"
				 << "\n";
	mState = 1;
}
void GuiUpdate::onNoUpdateAvailable()
{
	mLoading = false;
	LOG(LogInfo) << "no update available"
				 << "\n";
	mState = 6;
}
void GuiUpdate::onPingError()
{
	LOG(LogInfo) << "ping nok"
				 << "\n";
	mLoading = false;
	mState = 3;
}
void GuiUpdate::onUpdateError(std::pair<std::string, int> result)
{
	mLoading = false;
	mState = 5;
	mResult = result;
	mResult.first = _("AN ERROR OCCURED") + std::string(": ") + mResult.first;
}

void GuiUpdate::onUpdateOk()
{
	mLoading = false;
	mState = 4;
}
#endif
