#pragma once
#include "GuiComponent.h"
#include "components/ImageComponent.h"

// Defines a very simple "on/off" switch.
class SwitchComponent : public GuiComponent
{
public:
	SwitchComponent(Window* window, bool state = false);

	bool input(InputConfig* config, Input input) override;
	void render(const Eigen::Affine3f& parentTrans) override;
	void onSizeChanged() override;

	bool getState() const;
	void setState(bool state);
	std::string getValue() const override;
	bool changed();

	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	void updateState();

	ImageComponent mImage;
	bool mState;
	bool mInitialState;
};
