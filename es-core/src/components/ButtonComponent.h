#pragma once

#include "GuiComponent.h"
#include "components/NinePatchComponent.h"
#include "resources/Font.h"
#include <functional>
#include <string>

class ButtonComponent : public GuiComponent
{
public:
	ButtonComponent(Window* window, const std::string& text = std::string(), const std::string& helpText = std::string(), const std::function<void()>& func = nullptr);

	void setPressedFunc(std::function<void()> f);

	void setEnabled(bool enable);

	bool input(InputConfig* config, Input input) override;
	void render(const Eigen::Affine3f& parentTrans) override;

	void setText(const std::string& text, const std::string& helpText);

	inline const std::string& getText() const
	{
		return mText;
	};
	inline const std::function<void()>& getPressedFunc() const
	{
		return mPressedFunc;
	};

	void onSizeChanged() override;
	void onFocusGained() override;
	void onFocusLost() override;
#if defined(EXTENSION)
	void setColorShift(unsigned int color)
	{
		mModdedColor = color;
		mNewColor = true;
		updateImage();
	}
	void removeColorShift()
	{
		mNewColor = false;
		updateImage();
	}
#endif
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	std::shared_ptr<Font> mFont;
	std::function<void()> mPressedFunc;

	bool mFocused;
	bool mEnabled;
#if defined(EXTENSION)
	bool mNewColor = false;
#endif
	unsigned int mTextColorFocused;
	unsigned int mTextColorUnfocused;
#if defined(EXTENSION)
	unsigned int mModdedColor;
#endif

	unsigned int getCurTextColor() const;
	void updateImage();

	std::string mText;
	std::string mHelpText;
	std::unique_ptr<TextCache> mTextCache;
	NinePatchComponent mBox;
};
