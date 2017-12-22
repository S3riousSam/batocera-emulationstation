#include "components/ScrollableContainer.h"
#include "Renderer.h"

namespace
{
	const int AUTO_SCROLL_RESET_DELAY = 10000; // ms to reset to top after we reach the bottom
	const int AUTO_SCROLL_DELAY = 8000; // ms to wait before we start to scroll
	const int AUTO_SCROLL_SPEED = 50; // ms between scrolls

	// this should probably return a box to allow for when controls don't start at 0,0
	Eigen::Vector2f getContentSize(const std::vector<GuiComponent*>& children)
	{
		Eigen::Vector2f max(0, 0);
		for (const auto& child : children)
		{
			const Eigen::Vector2f pos(child->getPosition()[0], child->getPosition()[1]);
			const Eigen::Vector2f bottomRight = child->getSize() + pos;
			if (bottomRight.x() > max.x())
				max.x() = bottomRight.x();
			if (bottomRight.y() > max.y())
				max.y() = bottomRight.y();
		}

		return max;
	}
}

ScrollableContainer::ScrollableContainer(Window* window)
	: GuiComponent(window)
	, mAutoScrollDelay(0)
	, mAutoScrollSpeed(0)
	, mAutoScrollAccumulator(0)
	, mScrollPos(0, 0)
	, mScrollDir(0, 0)
	, mAutoScrollResetAccumulator(0)
	, mAtEnd(false) // it was missing!?
{
}

void ScrollableContainer::render(const Eigen::Affine3f& parentTrans)
{
	Eigen::Affine3f trans = parentTrans * getTransform();

	const Eigen::Vector2i clipPos(static_cast<int>(trans.translation().x()), static_cast<int>(trans.translation().y()));
	const Eigen::Vector3f dimScaled = trans * Eigen::Vector3f(mSize.x(), mSize.y(), 0);
	const Eigen::Vector2i clipDim(static_cast<int>(dimScaled.x() - trans.translation().x()), static_cast<int>(dimScaled.y() - trans.translation().y()));

	Renderer::pushClipRect(clipPos, clipDim);

	trans.translate(-Eigen::Vector3f(mScrollPos.x(), mScrollPos.y(), 0));
	Renderer::setMatrix(trans);

	GuiComponent::renderChildren(trans);

	Renderer::popClipRect();
}

void ScrollableContainer::setAutoScroll(bool autoScroll)
{
	if (autoScroll)
	{
		mScrollDir << 0, 1;
		mAutoScrollDelay = AUTO_SCROLL_DELAY;
		mAutoScrollSpeed = AUTO_SCROLL_SPEED;
		reset();
	}
	else
	{
		mScrollDir << 0, 0;
		mAutoScrollDelay = 0;
		mAutoScrollSpeed = 0;
		mAutoScrollAccumulator = 0;
	}
}

#if defined(USEFUL)
Eigen::Vector2f ScrollableContainer::getScrollPos() const
{
	return mScrollPos;
}

void ScrollableContainer::setScrollPos(const Eigen::Vector2f& pos)
{
	mScrollPos = pos;
}
#endif

void ScrollableContainer::update(int deltaTime)
{
	if (mAutoScrollSpeed != 0)
	{
		mAutoScrollAccumulator += deltaTime;

		// scale speed by our width! more text per line = slower scrolling
		//const float widthMod = (680.0f / getSize().x()); // unused
		while (mAutoScrollAccumulator >= mAutoScrollSpeed)
		{
			mScrollPos += mScrollDir;
			mAutoScrollAccumulator -= mAutoScrollSpeed;
		}
	}

	// clip scrolling within bounds
	if (mScrollPos.x() < 0)
		mScrollPos[0] = 0;
	if (mScrollPos.y() < 0)
		mScrollPos[1] = 0;

	const Eigen::Vector2f contentSize = getContentSize(mChildren);
	if (mScrollPos.x() + getSize().x() > contentSize.x())
	{
		mScrollPos[0] = contentSize.x() - getSize().x();
		mAtEnd = true;
	}

	if (contentSize.y() < getSize().y())
	{
		mScrollPos[1] = 0;
	}
	else if (mScrollPos.y() + getSize().y() > contentSize.y())
	{
		mScrollPos[1] = contentSize.y() - getSize().y();
		mAtEnd = true;
	}

	if (mAtEnd)
	{
		mAutoScrollResetAccumulator += deltaTime;
		if (mAutoScrollResetAccumulator >= AUTO_SCROLL_RESET_DELAY)
			reset();
	}

	GuiComponent::update(deltaTime);
}

void ScrollableContainer::reset()
{
	mScrollPos << 0, 0;
	mAutoScrollResetAccumulator = 0;
	mAutoScrollAccumulator = -mAutoScrollDelay + mAutoScrollSpeed;
	mAtEnd = false;
}
