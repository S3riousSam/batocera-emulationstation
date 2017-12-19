#if defined(EXTENSION)
#include "guis/GuiUpdate.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"
#include "Log.h"
#include "Settings.h"
#include "SystemInterface.h"
#include <boost/thread.hpp>
#include <string>

GuiUpdate::GuiUpdate(Window* window)
	: GuiComponent(window)
	, mBusyAnim(window)
	, mLoading(true)
	, mState(State::Null)
	, mHandle(nullptr)
	, mPingHandle(nullptr)
{
	setSize(Renderer::getScreenSize());
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

void GuiUpdate::render(const Eigen::Affine3f& parentTrans)
{
	const Eigen::Affine3f trans = parentTrans * getTransform();

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

	if (mState == State::Initial)
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("REALLY UPDATE?"), _("YES"),
			[this] {
				mState = State::Downloading;
				mLoading = true;
				mHandle = new boost::thread(boost::bind(&GuiUpdate::threadUpdate, this));
			},
			_("NO"), [this] { mState = State::Done; }));
		mState = State::Waiting;
	}

	if (mState == State::PingError)
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("NETWORK CONNECTION NEEDED"), _("OK"), [this] { mState = State::Done; }));
		mState = State::Waiting;
	}
	if (mState == State::UpdateReady)
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("UPDATE DOWNLOADED, THE SYSTEM WILL NOW REBOOT"), _("OK"), [this] {
			if (runRestartCommand() != 0)
			{
				LOG(LogWarning) << "Reboot terminated with non-zero result!";
			}
		}));
		mState = State::Waiting;
	}
	if (mState == State::UpdateFailed)
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, mResult.first, _("OK"), [this] { mState = State::Done; }));
		mState = State::Waiting;
	}
	if (mState == State::NoUpdate)
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("NO UPDATE AVAILABLE"), _("OK"), [this] { mState = State::Done; }));
		mState = State::Waiting;
	}

	if (mState == State::Done)
		delete this;
}

void GuiUpdate::threadUpdate()
{
	const std::pair<std::string, int> updateStatus = SystemInterface::updateSystem(mBusyAnim);
	if (updateStatus.second == 0)
	{
		mLoading = false;
		mState = State::UpdateReady;
	}
	else
	{
		mLoading = false;
		mState = State::UpdateFailed;
		mResult = updateStatus;
		mResult.first = _("AN ERROR OCCURED") + std::string(": ") + mResult.first;
	}
}

void GuiUpdate::threadPing()
{
	if (SystemInterface::ping())
	{
		if (SystemInterface::canUpdate())
		{
			mLoading = false;
			LOG(LogInfo) << "update available";
			mState = State::Initial;
		}
		else
		{
			mLoading = false;
			LOG(LogInfo) << "no update available";
			mState = State::NoUpdate;
		}
	}
	else // ping error
	{
		mLoading = false;
		LOG(LogInfo) << "ping nok\n";
		mState = State::PingError;
	}
}

#endif
