#include "guis/GuiInputConfig.h"
#if defined(EXTENSION)
#include "InputManager.h"
#endif
#include "LocaleES.h"
#include "Log.h"
#include "Util.h"
#include "Window.h"
#include "components/ButtonComponent.h"
#include "components/ImageComponent.h"
#include "components/MenuComponent.h"
#include "components/TextComponent.h"

namespace
{
	struct InputInfo
	{
		const char* const name;
		const bool skippable;
		const std::string label;
		const char* const icon;
		const bool axis;
	} const g_INPUTS[] = {{"Up", false, "UP", ":/help/dpad_up.svg", false}, {"Down", false, "DOWN", ":/help/dpad_down.svg", false},
		{"Left", false, "LEFT", ":/help/dpad_left.svg", false}, {"Right", false, "RIGHT", ":/help/dpad_right.svg", false},
		{"Joystick1Up", true, _("JOYSTICK 1 UP"), ":/help/joystick_up.svg", true},
		{"Joystick1Left", true, _("JOYSTICK 1 LEFT"), ":/help/joystick_left.svg", true},
		{"Joystick2Up", true, _("JOYSTICK 2 UP"), ":/help/joystick_up.svg", true},
		{"Joystick2Left", true, _("JOYSTICK 2 LEFT"), ":/help/joystick_left.svg", true}, {"A", false, "A", ":/help/button_a.svg", false},
		{"B", false, "B", ":/help/button_b.svg", false}, {"X", true, "X", ":/help/button_x.svg", false},
		{"Y", true, "Y", ":/help/button_y.svg", false}, {"Start", false, "START", ":/help/button_start.svg", false},
		{"Select", false, "SELECT", ":/help/button_select.svg", false}, {"PageUp", true, _("PAGE UP"), ":/help/button_l.svg", false},
		{"PageDown", true, _("PAGE DOWN"), ":/help/button_r.svg", false}, {"L2", true, "L2", ":/help/button_l2.svg", false},
		{"R2", true, "R2", ":/help/button_r2.svg", false}, {"L3", true, "L3", ":/help/button_l3.svg", false},
		{"R3", true, "R3", ":/help/button_r3.svg", false}, {"HotKey", false, _("HOTKEY"), ":/help/button_hotkey.svg", false}};

	static_assert(sizeof(g_INPUTS) / sizeof(g_INPUTS[0]) == 21ull, "Data structure expects 10 elements");
	static const size_t INPUT_COUNT = sizeof(g_INPUTS) / sizeof(g_INPUTS[0]);
} // namespace

// MasterVolUp and MasterVolDown are also hooked up, but do not appear on this screen.
// If you want, you can manually add them to es_input.cfg.

using namespace Eigen;

#define HOLD_TO_SKIP_MS 1000

GuiInputConfig::GuiInputConfig(Window* window, InputConfig* target, bool reconfigureAll, const std::function<void()>& okCallback)
	: GuiComponent(window)
	, mBackground(window, ":/frame.png")
	, mGrid(window, Vector2i(1, 7))
	, mTargetConfig(target)
	, mHoldingInput(false)
{
	LOG(LogInfo) << "Configuring device " << target->getDeviceId() << " (" << target->getDeviceName() << ").";

	if (reconfigureAll)
		target->clear();

	mConfiguringAll = reconfigureAll;
	mConfiguringRow = mConfiguringAll;

	addChild(&mBackground);
	addChild(&mGrid);

	// 0 is a spacer row
	mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Vector2i(0, 0), false);

	mTitle = std::make_shared<TextComponent>(mWindow, _("CONFIGURING"), Font::get(FONT_SIZE_LARGE), 0x555555FF, ALIGN_CENTER);
	mGrid.setEntry(mTitle, Vector2i(0, 1), false, true);

#if defined(EXTENSION)
	char strbuf[256];
	if (target->getDeviceId() == DEVICE_KEYBOARD)
		strncpy(strbuf, _("KEYBOARD").c_str(), 256);
	else
		snprintf(strbuf, 256, _("GAMEPAD %i").c_str(), target->getDeviceId() + 1);

	mSubtitle1 = std::make_shared<TextComponent>(mWindow, strToUpper(strbuf), Font::get(FONT_SIZE_MEDIUM), 0x555555FF, ALIGN_CENTER);
#else
	std::stringstream ss;
	if (target->getDeviceId() == DEVICE_KEYBOARD)
		ss << "KEYBOARD";
	else
		ss << "GAMEPAD " << (target->getDeviceId() + 1);

	mSubtitle1 = std::make_shared<TextComponent>(mWindow, strToUpper(ss.str()), Font::get(FONT_SIZE_MEDIUM), 0x555555FF, ALIGN_CENTER);
#endif
	mGrid.setEntry(mSubtitle1, Vector2i(0, 2), false, true);

	mSubtitle2 = std::make_shared<TextComponent>(mWindow, _("HOLD ANY BUTTON TO SKIP"), Font::get(FONT_SIZE_SMALL), 0x99999900, ALIGN_CENTER);
	mGrid.setEntry(mSubtitle2, Vector2i(0, 3), false, true);

	// 4 is a spacer row

	mList = std::make_shared<ComponentList>(mWindow);
	mGrid.setEntry(mList, Vector2i(0, 5), true, true);
#if defined(EXTENSION)
	const bool hasAxis = InputManager::getInstance()->getAxisCountByDevice(target->getDeviceId()) > 0;
#endif
	int inputRowIndex = 0;
	for (int i = 0; i < INPUT_COUNT; i++)
	{
#if defined(EXTENSION)
		if (g_INPUTS[i].axis && !hasAxis)
		{
			continue;
		}
#endif
		ComponentListRow row;

		// icon
		auto icon = std::make_shared<ImageComponent>(mWindow);
		icon->setImage(g_INPUTS[i].icon);
		icon->setColorShift(0x777777FF);
		icon->setResize(0, Font::get(FONT_SIZE_MEDIUM)->getLetterHeight() * 1.25f);
		row.addElement(icon, false);

		// spacer between icon and text
		auto spacer = std::make_shared<GuiComponent>(mWindow);
		spacer->setSize(16, 0);
		row.addElement(spacer, false);

		auto text = std::make_shared<TextComponent>(mWindow, g_INPUTS[i].label, Font::get(FONT_SIZE_MEDIUM), 0x777777FF);
		row.addElement(text, true);

		auto mapping =
			std::make_shared<TextComponent>(mWindow, _("-NOT DEFINED-"), Font::get(FONT_SIZE_MEDIUM, FONT_PATH_LIGHT), 0x999999FF, ALIGN_RIGHT);
		setNotDefined(mapping); // overrides text and color set above
		row.addElement(mapping, true);
		mMappings.push_back(mapping);

		row.input_handler = [this, i, inputRowIndex, mapping](InputConfig* config, Input input) -> bool {
			// ignore input not from our target device
			if (config != mTargetConfig)
				return false;

			// if we're not configuring, start configuring when A is pressed
			if (!mConfiguringRow)
			{
				if (config->isMappedTo("a", input) && input.value)
				{
					mList->stopScrolling();
					mConfiguringRow = true;
					setPress(mapping);
					return true;
				}

				// we're not configuring and they didn't press A to start, so ignore this
				return false;
			}

			// we are configuring
			if (input.value != 0)
			{
				// input down
				// if we're already holding something, ignore this, otherwise plan to map this input
				if (mHoldingInput)
					return true;

				mHoldingInput = true;
				mHeldInput = input;
				mHeldTime = 0;
				mHeldInputRowIndex = inputRowIndex;
				mHeldInputId = i;

				return true;
			}
			else
			{
				// input up
				// make sure we were holding something and we let go of what we were previously holding
				if (!mHoldingInput || mHeldInput.device != input.device || mHeldInput.id != input.id || mHeldInput.type != input.type)
					return true;

				mHoldingInput = false;

				if (assign(mHeldInput, i, inputRowIndex))
					rowDone(); // if successful, move cursor/stop configuring - if not, we'll just try again

				return true;
			}
		};

		mList->addRow(row);
		inputRowIndex++;
	}

	// only show "HOLD TO SKIP" if this input is skippable
	mList->setCursorChangedCallback([this](CursorState state) {
        mSubtitle2->setOpacity(g_INPUTS[mList->getCursorId()].skippable * 255);
	});

	// make the first one say "PRESS ANYTHING" if we're re-configuring everything
	if (mConfiguringAll)
		setPress(mMappings.front());

	// buttons
	std::vector<std::shared_ptr<ButtonComponent>> buttons;
	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("OK"), _("OK"), [this, okCallback] {
		InputManager::getInstance()->writeDeviceConfig(mTargetConfig); // save
		if (okCallback)
			okCallback();
		delete this;
	}));
	mButtonGrid = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 6), true, false);

	setSize(Renderer::getScreenWidth() * 0.6f, Renderer::getScreenHeight() * 0.75f);
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
}

void GuiInputConfig::onSizeChanged()
{
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	// update grid
	mGrid.setSize(mSize);

	// mGrid.setRowHeightPerc(0, 0.025f);
	mGrid.setRowHeightPerc(1, mTitle->getFont()->getHeight() * 0.75f / mSize.y());
	mGrid.setRowHeightPerc(2, mSubtitle1->getFont()->getHeight() / mSize.y());
	mGrid.setRowHeightPerc(3, mSubtitle2->getFont()->getHeight() / mSize.y());
	// mGrid.setRowHeightPerc(4, 0.03f);
	mGrid.setRowHeightPerc(5, (mList->getRowHeight(0) * 5 + 2) / mSize.y());
	mGrid.setRowHeightPerc(6, mButtonGrid->getSize().y() / mSize.y());
}

void GuiInputConfig::update(int deltaTime)
{
	if (mConfiguringRow && mHoldingInput && g_INPUTS[mHeldInputId].skippable)
	{
		int prevSec = mHeldTime / 1000;
		mHeldTime += deltaTime;
		int curSec = mHeldTime / 1000;

		if (mHeldTime >= HOLD_TO_SKIP_MS)
		{
			setNotDefined(mMappings.at(mHeldInputRowIndex));
			clearAssignment(mHeldInputId);
			mHoldingInput = false;
			rowDone();
		}
		else
		{
			if (prevSec != curSec)
			{
				// crossed the second boundary, update text
#if defined(EXTENSION)
				const auto& text = mMappings.at(mHeldInputRowIndex);
				char strbuf[256];
				snprintf(strbuf, 256, boost::locale::ngettext("HOLD FOR %iS TO SKIP", "HOLD FOR %iS TO SKIP", HOLD_TO_SKIP_MS / 1000 - curSec).c_str(),
					HOLD_TO_SKIP_MS / 1000 - curSec);
				text->setText(strbuf);
#else
				const auto& text = mMappings.at(mHeldInputId);
				std::stringstream ss;
				ss << "HOLD FOR " << HOLD_TO_SKIP_MS / 1000 - curSec << "S TO SKIP";
				text->setText(ss.str());
#endif
				text->setColor(0x777777FF);
			}
		}
	}
}

// move cursor to the next thing if we're configuring all,
// or come out of "configure mode" if we were only configuring one row
void GuiInputConfig::rowDone()
{
	if (mConfiguringAll)
	{
		if (!mList->moveCursor(1)) // try to move to the next one
		{
			// at bottom of list, done
			mConfiguringAll = false;
			mConfiguringRow = false;
			mGrid.moveCursor(Vector2i(0, 1));
		}
		else
		{
			// on another one
			setPress(mMappings.at(mList->getCursorId()));
		}
	}
	else
	{
		// only configuring one row, so stop
		mConfiguringRow = false;
	}
}

#if defined(RECALBOX_EX)
struct ColorSef
{
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
#define COLOR_GRAY4 0x656565FF // Darker
#endif

void GuiInputConfig::setPress(const std::shared_ptr<TextComponent>& text)
{
	text->setText(_("PRESS ANYTHING"));
	text->setColor(0x656565FF);
}

void GuiInputConfig::setNotDefined(const std::shared_ptr<TextComponent>& text)
{
	text->setText(_("-NOT DEFINED-"));
	text->setColor(0x999999FF);
}

void GuiInputConfig::setAssignedTo(const std::shared_ptr<TextComponent>& text, Input input)
{
	text->setText(strToUpper(input.string()));
	text->setColor(0x777777FF);
}

void GuiInputConfig::error(const std::shared_ptr<TextComponent>& text, const std::string& msg)
{
	text->setText(_("ALREADY TAKEN"));
	text->setColor(0x656565FF);
}

bool GuiInputConfig::assign(Input input, int inputId, int inputIndex)
{
	// input is from InputConfig* mTargetConfig

	// if this input is mapped to something other than "nothing" or the current row, error
	// (if it's the same as what it was before, allow it)
	if (std::string("HotKey").compare(g_INPUTS[inputId].name) != 0)
		if (mTargetConfig->getMappedTo(input).size() > 0 && !mTargetConfig->isMappedTo(g_INPUTS[inputId].name, input))
		{
			error(mMappings.at(inputIndex), "Already mapped!");
			return false;
		}

	setAssignedTo(mMappings.at(inputIndex), input);

	input.configured = true;
	mTargetConfig->mapInput(g_INPUTS[inputId].name, input);

	LOG(LogInfo) << "  Mapping [" << input.string() << "] -> " << g_INPUTS[inputId].name;

	return true;
}

void GuiInputConfig::clearAssignment(int inputId)
{
	mTargetConfig->unmapInput(g_INPUTS[inputId].name);
}
