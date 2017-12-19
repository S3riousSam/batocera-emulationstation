#if defined(EXTENSION)
#include "guis/GuiAutoScrape.h"
#include "guis/GuiMsgBox.h"
#include "LocaleES.h"
#include "Gamelist.h"
#include "SystemData.h"
#include "views/ViewController.h"
#include "Settings.h"
#include "SystemInterface.h"
#include "Window.h"
#include <boost/thread.hpp>
#include <string>

GuiAutoScrape::GuiAutoScrape(Window* window)
	: GuiComponent(window)
	, mBusyAnim(window)
	, mState(State::Initial)
	, mHandle(nullptr)
{
	setSize(Renderer::getScreenSize());
	mLoading = true;

	mBusyAnim.setSize(mSize);
}

GuiAutoScrape::~GuiAutoScrape()
{
	// view type probably changed (basic -> detailed)
	for (auto it : SystemData::sSystemVector)
	{
		parseGamelist(it);
		ViewController::get()->reloadGameListView(it, false);
	}

	delete mHandle; // TEST
}

bool GuiAutoScrape::input(InputConfig* config, Input input)
{
	return false;
}

void GuiAutoScrape::render(const Eigen::Affine3f& parentTrans)
{
	const Eigen::Affine3f trans = parentTrans * getTransform();

	renderChildren(trans);

	Renderer::setMatrix(trans);
	Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

	if (mLoading)
		mBusyAnim.render(trans);
}

void GuiAutoScrape::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	mBusyAnim.update(deltaTime);

	if (mState == State::Initial)
	{
		mLoading = true;
		mHandle = new boost::thread(boost::bind(&GuiAutoScrape::threadAutoScrape, this));
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

void GuiAutoScrape::threadAutoScrape()
{
	const std::pair<std::string, int> scrapeStatus = SystemInterface::scrape(mBusyAnim);
	if (scrapeStatus.second == 0)
	{
		mLoading = false;
		mState = State::Success;
	}
	else
	{
		mLoading = false;
		mState = State::Error;
		mResult = scrapeStatus;
		mResult.first = _("AN ERROR OCCURED") + std::string(": ") + mResult.first;
	}
}

#endif