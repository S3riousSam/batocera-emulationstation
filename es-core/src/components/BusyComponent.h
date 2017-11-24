#pragma once
#include "GuiComponent.h"
#include "components/ComponentGrid.h"
#include "components/NinePatchComponent.h"

class AnimatedImageComponent;
class TextComponent;

class BusyComponent : public GuiComponent
{
public:
	BusyComponent(Window* window);
#if defined(EXTENSION)
	~BusyComponent();
#endif

	void onSizeChanged() override;
#if defined(EXTENSION)
	void setText(const std::string& txt);
#endif
	void reset(); // reset to frame 0
#if defined(EXTENSION)
	virtual void render(const Eigen::Affine3f& parentTrans);
#endif
private:
	NinePatchComponent mBackground;
	ComponentGrid mGrid;

	std::shared_ptr<AnimatedImageComponent> mAnimation;
	std::shared_ptr<TextComponent> mText;
#if defined(EXTENSION)
	SDL_mutex* mutex;
	bool threadMessagechanged;
	std::string threadMessage;
#endif
};
