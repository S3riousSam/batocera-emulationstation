#include "components/SliderComponent.h"
#include "LocaleES.h"
#include "Log.h"
#include "Renderer.h"
#include "Util.h"
#include "resources/Font.h"
#include <assert.h>

SliderComponent::SliderComponent(Window* window, float min, float max, float increment, const std::string& suffix)
	: GuiComponent(window)
	, mMin(min)
	, mMax(max)
	, mValue((max + min) / 2) // some sane default value
	, mSingleIncrement(increment)
	, mMoveRate(0)
	, mKnob(window)
	, mSuffix(suffix)
{
	assert((min - max) != 0);

	mKnob.setOrigin(0.5f, 0.5f);
	mKnob.setImage(":/slider_knob.svg");

	setSize(Renderer::getScreenWidth() * 0.15f, Font::get(FONT_SIZE_MEDIUM)->getLetterHeight());
}

bool SliderComponent::input(InputConfig* config, Input input)
{
	const int MOVE_REPEAT_DELAY = 500;

	if (config->isMappedTo("left", input))
	{
		if (input.value)
			setValue(mValue - mSingleIncrement);

		mMoveRate = input.value ? -mSingleIncrement : 0;
		mMoveAccumulator = -MOVE_REPEAT_DELAY;
		return true;
	}
	if (config->isMappedTo("right", input))
	{
		if (input.value)
			setValue(mValue + mSingleIncrement);

		mMoveRate = input.value ? mSingleIncrement : 0;
		mMoveAccumulator = -MOVE_REPEAT_DELAY;
		return true;
	}

	return GuiComponent::input(config, input);
}

void SliderComponent::update(int deltaTime)
{
	const int MOVE_REPEAT_RATE = 40;

	if (mMoveRate != 0)
	{
		mMoveAccumulator += deltaTime;
		while (mMoveAccumulator >= MOVE_REPEAT_RATE)
		{
			setValue(mValue + mMoveRate);
			mMoveAccumulator -= MOVE_REPEAT_RATE;
		}
	}

	GuiComponent::update(deltaTime);
}

void SliderComponent::render(const Eigen::Affine3f& parentTrans)
{
	Eigen::Affine3f trans = roundMatrix(parentTrans * getTransform());
	Renderer::setMatrix(trans);

	// render suffix
	if (mValueCache)
		mFont->renderTextCache(mValueCache.get());

	const float width = mSize.x() - mKnob.getSize().x() - (mValueCache ? mValueCache->metrics.size.x() + 4 : 0);

	// render line
	const float LINE_WIDTH = 2;
	Renderer::drawRect(mKnob.getSize().x() / 2, mSize.y() / 2 - LINE_WIDTH / 2, width, LINE_WIDTH, COLOR_GRAY3);

	// render knob
	mKnob.render(trans);

	GuiComponent::renderChildren(trans);
}

void SliderComponent::setValue(float value)
{
	mValue = value;
	if (mValue < mMin)
		mValue = mMin;
	else if (mValue > mMax)
		mValue = mMax;

	updateValue();
}

float SliderComponent::getValue()
{
	return mValue;
}

void SliderComponent::onSizeChanged()
{
	if (!mSuffix.empty())
		mFont = Font::get((int)(mSize.y()), FONT_PATH_LIGHT);

	updateValue();
}

void SliderComponent::updateValue()
{
	// update suffix textcache
	if (mFont)
	{
		std::stringstream ss;
		ss << std::fixed;
		ss.precision(0);
		ss << mValue;
		ss << mSuffix;
		const std::string val = ss.str();

		ss.str("");
		ss.clear();
		ss << std::fixed;
		ss.precision(0);
		ss << mMax;
		ss << mSuffix;

		const Eigen::Vector2f textSize = mFont->sizeText(ss.str());
		mValueCache = std::shared_ptr<TextCache>(mFont->buildTextCache(val, mSize.x() - textSize.x(), (mSize.y() - textSize.y()) / 2, COLOR_GRAY3));
		mValueCache->metrics.size[0] = textSize.x(); // fudge the width
	}

	// update knob position/size
	mKnob.setResize(0, mSize.y() * 0.7f);
	const float lineLength = mSize.x() - mKnob.getSize().x() - (mValueCache ? mValueCache->metrics.size.x() + 4 : 0);
	mKnob.setPosition(((mValue + mMin) / mMax) * lineLength + mKnob.getSize().x() / 2, mSize.y() / 2);
}

std::vector<HelpPrompt> SliderComponent::getHelpPrompts()
{
	return {HelpPrompt("left/right", _("CHANGE"))};
}
