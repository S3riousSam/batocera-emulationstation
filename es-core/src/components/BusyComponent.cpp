#include "BusyComponent.h"
#include "LocaleES.h"
#include "Renderer.h"
#include "components/AnimatedImageComponent.h"
#include "components/TextComponent.h"

BusyComponent::BusyComponent(Window* window)
	: GuiComponent(window)
	, mBackground(window, ":/frame.png")
	, mGrid(window, Eigen::Vector2i(5, 3))
{
	// animation definition
	static const AnimationDef::AnimationFrame BUSY_ANIMATION_FRAMES[] = {
		{":/busy_0.svg", 300},
		{":/busy_1.svg", 300},
		{":/busy_2.svg", 300},
		{":/busy_3.svg", 300},
	};
	static const AnimationDef BUSY_ANIMATION_DEF = {BUSY_ANIMATION_FRAMES, 4, true};

#if defined(EXTENSION)
	mutex = SDL_CreateMutex();
#endif
	mAnimation = std::make_shared<AnimatedImageComponent>(mWindow);
	mAnimation->load(&BUSY_ANIMATION_DEF);
	mText = std::make_shared<TextComponent>(mWindow, _("WORKING..."), Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);

	// col 0 = animation, col 1 = spacer, col 2 = text
	mGrid.setEntry(mAnimation, Eigen::Vector2i(1, 1), false, true);
	mGrid.setEntry(mText, Eigen::Vector2i(3, 1), false, true);

	addChild(&mBackground);
	addChild(&mGrid);
}

#if defined(EXTENSION)
BusyComponent::~BusyComponent()
{
	SDL_DestroyMutex(mutex);
}

void BusyComponent::setText(const std::string& txt)
{
	if (SDL_LockMutex(mutex) == 0)
	{
		threadMessage = txt;
		threadMessagechanged = true;
		SDL_UnlockMutex(mutex);
	}
}

void BusyComponent::render(const Eigen::Affine3f& parentTrans)
{
	if (SDL_LockMutex(mutex) == 0)
	{
		if (threadMessagechanged)
		{
			threadMessagechanged = false;
			mText->setText(threadMessage);
			onSizeChanged();
		}
		SDL_UnlockMutex(mutex);
	}
	GuiComponent::render(parentTrans);
}
#endif

void BusyComponent::onSizeChanged()
{
	mGrid.setSize(mSize);

	if (mSize.x() == 0 || mSize.y() == 0)
		return;

	const float middleSpacerWidth = 0.01f * Renderer::getScreenWidth();
	const float textHeight = mText->getFont()->getLetterHeight();
	mText->setSize(0, textHeight);
	const float textWidth = mText->getSize().x() + 4;

	mGrid.setColWidthPerc(1, textHeight / mSize.x()); // animation is square
	mGrid.setColWidthPerc(2, middleSpacerWidth / mSize.x());
	mGrid.setColWidthPerc(3, textWidth / mSize.x());

	mGrid.setRowHeightPerc(1, textHeight / mSize.y());

	using Eigen::Vector2f;
	mBackground.fitTo(Vector2f(mGrid.getColWidth(1) + mGrid.getColWidth(2) + mGrid.getColWidth(3), textHeight + 2), mAnimation->getPosition(), Vector2f(0, 0));
}

void BusyComponent::reset()
{
	// mAnimation->reset();
}
