#if defined(EXTENSION) // Created by matthieu on 03/08/15.
#include "GuiLoading.h"
#include "Renderer.h"
#include "Window.h"
#include <boost/thread.hpp>

GuiLoading::GuiLoading(Window* window, const std::function<void*()>& mFunc)
	: GuiComponent(window)
	, mBusyAnim(window)
	, mFunc1(mFunc)
	, mFunc2(nullptr)
	, mRunning(true)
{
	GuiLoading(window, mFunc, nullptr);
}

GuiLoading::GuiLoading(Window* window, const std::function<void*()>& mFunc, const std::function<void(void*)>& mFunc2)
	: GuiComponent(window)
	, mBusyAnim(window)
    , mHandle(nullptr)
	, mFunc1(mFunc)
	, mFunc2(mFunc2)
	, mRunning(true)
{
	setSize(Renderer::getScreenSize());
	//mHandle = new boost::thread(boost::bind(&GuiLoading::threadLoading, this));
	mBusyAnim.setSize(mSize);
}

GuiLoading::~GuiLoading()
{
	mRunning = false;
	mHandle->join();
}

bool GuiLoading::input(InputConfig* config, Input input)
{
	return false;
}

void GuiLoading::render(const Eigen::Affine3f& parentTrans)
{
	const Eigen::Affine3f trans = parentTrans * getTransform();

	renderChildren(trans);

	Renderer::setMatrix(trans);
	Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);

	if (mRunning)
		mBusyAnim.render(trans);
}

void GuiLoading::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	mBusyAnim.update(deltaTime);

    if (mHandle == nullptr)
        mHandle = new boost::thread(boost::bind(&GuiLoading::threadLoading, this));

	if (!mRunning)
	{
		if (mFunc2 != nullptr)
			mFunc2(mResult);
		delete this;
	}
}

void GuiLoading::threadLoading()
{
	mResult = mFunc1();
	mRunning = false;
}
#endif