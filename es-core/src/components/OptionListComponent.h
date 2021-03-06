#pragma once
#include "GuiComponent.h"
#include "LocaleES.h"
#include "Log.h"
#include "Renderer.h"
#include "Window.h"
#include "components/ImageComponent.h"
#include "components/MenuComponent.h"
#include "components/TextComponent.h"
#include "resources/Font.h"
#include <sstream>

// Used to display a list of options (Can select one or multiple options)
// if !multiSelect
// * <- curEntry ->
// always
// * press a -> open full list
template<typename T>
class OptionListComponent : public GuiComponent
{
private:
	struct OptionListData
	{
		std::string name;
		T object;
		bool selected;
	};

	class OptionListPopup : public GuiComponent
	{
	private:
		MenuComponent mMenu;
		OptionListComponent<T>* mParent;

	public:
		OptionListPopup(Window* window, OptionListComponent<T>* parent, const std::string& title)
			: GuiComponent(window)
			, mMenu(window, title.c_str())
			, mParent(parent)
		{
			static const char* const CHECK_PATH_ON = ":/checkbox_checked.svg";
			static const char* const CHECK_PATH_OFF = ":/checkbox_unchecked.svg";

			auto font = Font::get(FONT_SIZE_MEDIUM);
			ComponentListRow row;

			// for select all/none
			std::vector<ImageComponent*> checkboxes;

			for (auto& it : mParent->mEntries)
			{
				row.elements.clear();
				row.addElement(std::make_shared<TextComponent>(mWindow, strToUpper(it.name), font, COLOR_GRAY3), true);

				if (mParent->mMultiSelect)
				{
					// add checkbox
					auto checkbox = std::make_shared<ImageComponent>(mWindow);
					checkbox->setImage(it.selected ? CHECK_PATH_ON : CHECK_PATH_OFF);
					checkbox->setResize(0, font->getLetterHeight());
					row.addElement(checkbox, false);

					// input handler
					// update checkbox state & selected value
					row.makeAcceptInputHandler([this, &it, checkbox] {
						it.selected = !it.selected;
						checkbox->setImage(it.selected ? CHECK_PATH_ON : CHECK_PATH_OFF);
						mParent->onSelectedChanged();
					});

					// for select all/none
					checkboxes.push_back(checkbox.get());
				}
				else
				{
					// input handler for non multi-select
					// update selected value and close
					row.makeAcceptInputHandler([this, &it] {
						mParent->mEntries.at(mParent->getSelectedId()).selected = false;
						it.selected = true;
						mParent->onSelectedChanged();
						delete this;
					});
				}

				// also set cursor to this row if we're not multi-select and this row is selected
				mMenu.addRow(row, (!mParent->mMultiSelect && it.selected));
			}

			mMenu.addButton(_("BACK"), "accept", [this] { delete this; });

			if (mParent->mMultiSelect)
			{
				mMenu.addButton(_("SELECT ALL"), "select all", [this, checkboxes] {
					for (unsigned int i = 0; i < mParent->mEntries.size(); i++)
					{
						mParent->mEntries.at(i).selected = true;
						checkboxes.at(i)->setImage(CHECK_PATH_ON);
					}
					mParent->onSelectedChanged();
				});

				mMenu.addButton(_("SELECT NONE"), "select none", [this, checkboxes] {
					for (unsigned int i = 0; i < mParent->mEntries.size(); i++)
					{
						mParent->mEntries.at(i).selected = false;
						checkboxes.at(i)->setImage(CHECK_PATH_OFF);
					}
					mParent->onSelectedChanged();
				});
			}

			mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.1f);
			addChild(&mMenu);
		}

		bool input(InputConfig* config, Input input) override
		{
			if (config->isMappedTo(BUTTON_BACK, input) && input.value != 0)
			{
				delete this;
				return true;
			}

			return GuiComponent::input(config, input);
		}

		std::vector<HelpPrompt> getHelpPrompts() override
		{
			auto prompts = mMenu.getHelpPrompts();
			prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
			return prompts;
		}
	};

public:
	OptionListComponent(Window* window, const std::string& name, bool multiSelect = false, unsigned int fontSize = FONT_SIZE_MEDIUM)
		: GuiComponent(window)
		, mMultiSelect(multiSelect)
		, mName(name)
		, mText(window)
		, mLeftArrow(window)
		, mRightArrow(window)
	{
		const auto font = Font::get(fontSize, FONT_PATH_LIGHT);
		mText.setFont(font);
		mText.setColor(COLOR_GRAY3);
		mText.setAlignment(ALIGN_CENTER);
		addChild(&mText);

		if (mMultiSelect)
		{
			mRightArrow.setImage(":/arrow.svg");
			addChild(&mRightArrow);
		}
		else
		{
			mLeftArrow.setImage(":/option_arrow.svg");
			mLeftArrow.setFlipX(true);
			addChild(&mLeftArrow);

			mRightArrow.setImage(":/option_arrow.svg");
			addChild(&mRightArrow);
		}

		setSize(mLeftArrow.getSize().x() + mRightArrow.getSize().x(), font->getHeight());
	}

	// handles positioning/resizing of text and arrows
	void onSizeChanged() override
	{
		mLeftArrow.setResize(0, mText.getFont()->getLetterHeight());
		mRightArrow.setResize(0, mText.getFont()->getLetterHeight());

		if (mSize.x() < (mLeftArrow.getSize().x() + mRightArrow.getSize().x()))
			LOG(LogWarning) << "OptionListComponent too narrow!";

		mText.setSize(mSize.x() - mLeftArrow.getSize().x() - mRightArrow.getSize().x(), mText.getFont()->getHeight());

		// position
		mLeftArrow.setPosition(0, (mSize.y() - mLeftArrow.getSize().y()) / 2);
		mText.setPosition(mLeftArrow.getPosition().x() + mLeftArrow.getSize().x(), (mSize.y() - mText.getSize().y()) / 2);
		mRightArrow.setPosition(mText.getPosition().x() + mText.getSize().x(), (mSize.y() - mRightArrow.getSize().y()) / 2);
	}

	bool input(InputConfig* config, Input input) override
	{
		if (input.value != 0)
		{
			if (config->isMappedTo(BUTTON_LAUNCH, input))
			{
				open();
				return true;
			}
			if (!mMultiSelect)
			{
				if (config->isMappedTo("left", input))
				{
					// move selection to previous
					const unsigned int i = getSelectedId();
					int next = (int)i - 1;
					if (next < 0)
						next += mEntries.size();

					mEntries.at(i).selected = false;
					mEntries.at(next).selected = true;
					onSelectedChanged();
					return true;
				}
				else if (config->isMappedTo("right", input))
				{
					// move selection to next
					const unsigned int i = getSelectedId();
					int next = (i + 1) % mEntries.size();
					mEntries.at(i).selected = false;
					mEntries.at(next).selected = true;
					onSelectedChanged();
					return true;
				}
			}
		}
		return GuiComponent::input(config, input);
	}

	std::vector<T> getSelectedObjects() const
	{
		std::vector<T> ret;
		for (auto it : mEntries)
		{
			if (it.selected)
				ret.push_back(it.object);
		}
		return ret;
	}

	T getSelected()
	{
		assert(mMultiSelect == false);
		const auto selected = getSelectedObjects();
		return (selected.size() == 1) ? selected.at(0) : T();
	}

#if defined(EXTENSION)
	std::string getSelectedName()
	{
		assert(mMultiSelect == false);
		for (unsigned int i = 0; i < mEntries.size(); i++)
		{
			if (mEntries.at(i).selected)
				return mEntries.at(i).name;
		}
		return std::string();
	}
#endif

	void add(const std::string& name, const T& obj, bool selected)
	{
		const OptionListData e = {name, obj, selected};
#if defined(EXTENSION)
		if (selected)
			firstSelected = obj;
#endif

		mEntries.push_back(e);
		onSelectedChanged();
	}

#if defined(EXTENSION)
	 void setSelectedChangedCallback(const std::function<void(const T&)>& callback)
	{
		mSelectedChangedCallback = callback;
	}

	bool changed()
	{
		return firstSelected != getSelected();
	}
#endif

private:
	unsigned int getSelectedId()
	{
		assert(mMultiSelect == false);
		for (unsigned int i = 0; i < mEntries.size(); i++)
		{
			if (mEntries.at(i).selected)
				return i;
		}

		LOG(LogWarning) << "OptionListComponent::getSelectedId() - no selected element found, defaulting to 0";
		return 0;
	}

	void open()
	{
		mWindow->pushGui(new OptionListPopup(mWindow, this, mName));
	}

	void onSelectedChanged()
	{
		if (mMultiSelect)
		{
			// display # selected
			char strbuf[256];
			int x = getSelectedObjects().size();
			snprintf(strbuf, 256, boost::locale::ngettext("%i SELECTED", "%i SELECTED", x).c_str(), x);
			mText.setText(strbuf);
			mText.setSize(0, mText.getSize().y());
			setSize(mText.getSize().x() + mRightArrow.getSize().x() + 24, mText.getSize().y());
			if (mParent != nullptr) // hack since theres no "on child size changed" callback atm...
				mParent->onSizeChanged();
		}
		else
		{
			// display currently selected + l/r cursors
			for (const auto& it : mEntries)
			{
				if (it.selected)
				{
					mText.setText(strToUpper(it.name));
					mText.setSize(0, mText.getSize().y());
					setSize(mText.getSize().x() + mLeftArrow.getSize().x() + mRightArrow.getSize().x() + 24, mText.getSize().y());
					if (mParent != nullptr) // hack since theres no "on child size changed" callback atm...
						mParent->onSizeChanged();
					break;
				}
			}
		}
#if defined(EXTENSION)
		if (mSelectedChangedCallback)
			mSelectedChangedCallback(mEntries.at(getSelectedId()).object);
#endif
	}

	std::vector<HelpPrompt> getHelpPrompts() override
	{
		std::vector<HelpPrompt> prompts;
		if (!mMultiSelect)
			prompts.push_back(HelpPrompt("left/right", _("CHANGE")));

		prompts.push_back(HelpPrompt(BUTTON_LAUNCH, _("SELECT")));
		return prompts;
	}

	bool mMultiSelect;

	std::string mName;
	T firstSelected;
	TextComponent mText;
	ImageComponent mLeftArrow;
	ImageComponent mRightArrow;

	std::vector<OptionListData> mEntries;
	std::function<void(const T&)> mSelectedChangedCallback;
};
