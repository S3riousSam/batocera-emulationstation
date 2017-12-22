#pragma once
#include "GuiComponent.h"

class ScrollableContainer : public GuiComponent
{
public:
	ScrollableContainer(Window* window);
#if defined(USEFUL)
	Eigen::Vector2f getScrollPos() const;
	void setScrollPos(const Eigen::Vector2f& pos);
#endif
	void setAutoScroll(bool autoScroll);
	void reset();

	void update(int deltaTime) override;
	void render(const Eigen::Affine3f& parentTrans) override;

private:
	int mAutoScrollDelay; // ms to wait before starting to auto-scroll
	int mAutoScrollSpeed; // ms to wait before scrolling down by mScrollDir
	int mAutoScrollResetAccumulator;
	int mAutoScrollAccumulator;

	Eigen::Vector2f mScrollPos;
	Eigen::Vector2f mScrollDir;
	bool mAtEnd;
};
