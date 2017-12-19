#include "SwitchComponent.h"
#include "LocaleES.h"
#include "Renderer.h"
#include "Window.h"
#include "resources/Font.h"
#include <Log.h>

SwitchComponent::SwitchComponent(Window* window, bool state)
	: GuiComponent(window)
	, mImage(window)
	, mState(state)
	, mInitialState(state)
{
	mImage.setImage(mState ? ":/on.svg" : ":/off.svg");
	mImage.setResize(0, Font::get(FONT_SIZE_MEDIUM)->getLetterHeight());
	mSize = mImage.getSize();
}

void SwitchComponent::onSizeChanged()
{
	mImage.setSize(mSize);
}

bool SwitchComponent::input(InputConfig* config, Input input)
{
	if (config->isMappedTo(BUTTON_LAUNCH, input) && input.value)
	{
		mState = !mState;
		updateState();
		return true;
	}

	return false;
}

void SwitchComponent::render(const Eigen::Affine3f& parentTrans)
{
	const Eigen::Affine3f trans = parentTrans * getTransform();
	mImage.render(trans);
	renderChildren(trans);
}

bool SwitchComponent::getState() const
{
	return mState;
}

void SwitchComponent::setState(bool state)
{
	mState = state;
	updateState();
}

void SwitchComponent::updateState()
{
	mImage.setImage(mState ? ":/on.svg" : ":/off.svg");
}

std::vector<HelpPrompt> SwitchComponent::getHelpPrompts()
{
	return {HelpPrompt(BUTTON_LAUNCH, _("CHANGE"))};
}

std::string SwitchComponent::getValue() const
{
	return mState ? "true" : "false";
}

bool SwitchComponent::changed()
{
	return mInitialState != mState;
}
