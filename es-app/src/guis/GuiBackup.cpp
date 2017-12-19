#if defined(EXTENSION)
#include "guis/GuiBackup.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"
#include "SystemInterface.h"
#include <boost/thread.hpp>
#include <string>

GuiBackup::GuiBackup(Window* window, const std::string& storageDevice)
	: GuiComponent(window)
	, mBusyAnim(window)
	, mLoading(true)
	, mState(State::Initial)
	, mstorageDevice(storageDevice)
	, mHandle(nullptr)
{
	setSize(Renderer::getScreenSize());
	mBusyAnim.setSize(mSize);
}

GuiBackup::~GuiBackup()
{
	delete mHandle; // TEST
}

bool GuiBackup::input(InputConfig* config, Input input)
{
	return false;
}

void GuiBackup::render(const Eigen::Affine3f& parentTrans)
{
	const Eigen::Affine3f trans = parentTrans * getTransform();

	renderChildren(trans);

	Renderer::setMatrix(trans);
	Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

	if (mLoading)
		mBusyAnim.render(trans);
}

void GuiBackup::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	mBusyAnim.update(deltaTime);

	if (mState == State::Initial)
	{
		mLoading = true;
		mHandle = new boost::thread(boost::bind(&GuiBackup::threadBackup, this));
		mState = State::Waiting;
	}
	if (mState == State::Success)
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, _("FINISHED"), _("OK"), [this] { mState = State::Done; }));
		mState = State::Waiting;
	}
	if (mState == State::Error)
	{
		mWindow->pushGui(new GuiMsgBox(mWindow, mResult.first, _("OK"), [this] { mState = State::Done; }));
		mState = State::Waiting;
	}

	if (mState == State::Done)
		delete this;
}

void GuiBackup::threadBackup()
{
	const std::pair<std::string, int> updateStatus = SystemInterface::backupSystem(mBusyAnim, mstorageDevice);
	if (updateStatus.second == 0)
	{
		mLoading = false;
		mState = State::Success;
	}
	else
	{
		mLoading = false;
		mState = State::Error;
		mResult = updateStatus;
		mResult.first = _("AN ERROR OCCURED") + std::string(": check the system/logs directory");
	}
}

#endif