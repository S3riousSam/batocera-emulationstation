#pragma once
#include "HelpStyle.h"
#include "InputConfig.h"
#include <Eigen/Dense>

class Window;
class Animation;
class AnimationController;
class ThemeData;
class Font;

typedef std::pair<std::string, std::string> HelpPrompt;

#define BUTTON_BACK "a"
#define BUTTON_LAUNCH "b"

#if defined(EXTENSION)
struct ColorSef
{
    typedef unsigned char BYTE;
    ColorSef(BYTE gray, BYTE opacity = 0xFF)
        : r(gray)
        , g(gray)
        , b(gray)
        , opacity(opacity)
    {
    }

    BYTE r, g, b, opacity;

    operator unsigned int() const
    {
        return (r << 24) & (g << 16) & (b << 8) & (opacity);
    }
} static const ColorGRAY1(0xC6), ColorGRAY2(0x99), ColorGRAY3(0x77), ColorGRAY4(0x65);

#define COLOR_GRAY1 0xC6C6C6FF // Light
#define COLOR_GRAY2 0x999999FF
#define COLOR_GRAY3 0x777777FF
#define COLOR_GRAYX 0x666666FF
#define COLOR_GRAY4 0x656565FF // Darker
#endif

class GuiComponent
{
public:
	GuiComponent(Window& window);
	GuiComponent(Window* window);
	virtual ~GuiComponent();

	virtual void textInput(const char* text);

	/// Handles input events.
	/// @return The method returns true if the input is consumed, false if it should be passed to other children.
	virtual bool input(InputConfig* config, Input input);

	/// Handles time passing. The default implementation calls updateSelf(deltaTime)
	/// and updateChildren(deltaTime). Inheriting classes should probably call
	/// GuiComponent::update(deltaTime) at some point (or at least updateSelf so animations work).
	virtual void update(int deltaTime);

	// Handles rendering requests.  The default implementation calls renderChildren(parentTrans * getTransform()).
	// Inheriting classes probably wants to override this like so:
	// 1. Calculate the new transform that your control will draw at with Eigen::Affine3f t = parentTrans * getTransform().
	// 2. Set the renderer to use that new transform as the model matrix - Renderer::setMatrix(t);
	// 3. Draw your component.
	// 4. Tell your children to render, based on your component's transform - renderChildren(t).
	virtual void render(const Eigen::Affine3f& parentTrans);

	Eigen::Vector3f getPosition() const;
	void setPosition(const Eigen::Vector3f& offset);
	void setPosition(float x, float y, float z = 0.0f);
	virtual void onPositionChanged(){};

	Eigen::Vector2f getSize() const;
	void setSize(const Eigen::Vector2f& size);
	void setSize(float w, float h);
	virtual void onSizeChanged(){};

	void setParent(GuiComponent* parent);
	GuiComponent* getParent() const;

	void addChild(GuiComponent* cmp);
	void removeChild(GuiComponent* cmp);
	void clearChildren();
	unsigned int getChildCount() const;
	GuiComponent* getChild(unsigned int i) const;

	// animation will be automatically deleted when it completes or is stopped.
	bool isAnimationPlaying(unsigned char slot) const;
	bool isAnimationReversed(unsigned char slot) const;
	int getAnimationTime(unsigned char slot) const;
	void setAnimation(Animation* animation, int delay = 0, std::function<void()> finishedCallback = nullptr, bool reverse = false, unsigned char slot = 0);
	bool stopAnimation(unsigned char slot);

	// Like stopAnimation, but doesn't call finishedCallback - only removes the animation, leaving things in
	// their current state.  Returns true if successful (an animation was in this slot).
	bool cancelAnimation(unsigned char slot);
	// Calls update(1.f) and finishedCallback, then deletes the animation - basically skips to the end.
	// Returns true if successful (an animation was in this slot).
	bool finishAnimation(unsigned char slot);

	bool advanceAnimation(unsigned char slot, unsigned int time); // Returns true if successful (an animation was in this slot).
	void stopAllAnimations();
	void cancelAllAnimations();

	virtual unsigned char getOpacity() const;
	virtual void setOpacity(unsigned char opacity);

	const Eigen::Affine3f& getTransform();

	virtual std::string getValue() const;
	virtual void setValue(const std::string& value);

	virtual void onFocusGained(){};
	virtual void onFocusLost(){};

	// Default implementation just handles <pos> and <size> tags as normalized float pairs.
	// You probably want to keep this behavior for any derived classes as well as add your own.
	virtual void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties);

	// Returns a list of help prompts.
	virtual std::vector<HelpPrompt> getHelpPrompts()
	{
		return std::vector<HelpPrompt>();
	};

	// Called whenever help prompts change.
	void updateHelpPrompts();

	virtual HelpStyle getHelpStyle();

	// Returns true if the component is busy doing background processing (e.g. HTTP downloads)
	bool isProcessing() const;

protected:
	void renderChildren(const Eigen::Affine3f& transform) const;
	void updateSelf(int deltaTime); // updates animations
	void updateChildren(int deltaTime); // updates animations

	unsigned char mOpacity;
	Window* mWindow;

	GuiComponent* mParent;
	std::vector<GuiComponent*> mChildren;

	Eigen::Vector3f mPosition;
	Eigen::Vector2f mSize;

	bool mIsProcessing;

public:
	const static unsigned char MAX_ANIMATIONS = 4;

private:
	Eigen::Affine3f mTransform; // Don't access this directly! Use getTransform()!
	AnimationController* mAnimationMap[MAX_ANIMATIONS];
};
