#if defined(EXTENSION)
#include "guis/GuiInstall.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"
#include "SystemInterface.h"
#include <boost/thread.hpp>
#include <string>

GuiInstall::GuiInstall(Window* window, const std::string& storageDevice, const std::string& architecture)
	: GuiComponent(window)
	, mBusyAnim(window)
	, mLoading(true)
	, mState(State::Initial)
	, mstorageDevice(storageDevice)
	, marchitecture(architecture)
	, mHandle(nullptr)
{
	setSize(Renderer::getScreenSize());
	mBusyAnim.setSize(mSize);
}

GuiInstall::~GuiInstall()
{
	delete mHandle; // TEST
}

bool GuiInstall::input(InputConfig* config, Input input)
{
	return false;
}

void GuiInstall::render(const Eigen::Affine3f& parentTrans)
{
	const Eigen::Affine3f trans = parentTrans * getTransform();

	renderChildren(trans);

	Renderer::setMatrix(trans);
	Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

	if (mLoading)
		mBusyAnim.render(trans);
}

void GuiInstall::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	mBusyAnim.update(deltaTime);

	if (mState == State::Initial)
	{
		mLoading = true;
		mHandle = new boost::thread(boost::bind(&GuiInstall::threadInstall, this));
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

void GuiInstall::threadInstall()
{
	const std::pair<std::string, int> updateStatus = SystemInterface::installSystem(mBusyAnim, mstorageDevice, marchitecture);
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