#include "FileSorts.h"
#include "GuiComponent.h"
#include "components/MenuComponent.h"
#include "components/OptionListComponent.h"
#if defined(EXTENSION)
#include "components/SwitchComponent.h"
#endif

class IGameListView;

class GuiGamelistOptions : public GuiComponent
{
public:
	GuiGamelistOptions(Window* window, SystemData* system);
	virtual ~GuiGamelistOptions();

#if defined(EXTENSION)
	void save();
	inline void addSaveFunc(const std::function<void()>& func)
	{
		mSaveFuncs.push_back(func);
	};
#endif
	virtual bool input(InputConfig* config, Input input) override;
	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
#if defined(MANUAL_SCRAPING)
	void openMetaDataEd();
#endif
	void jumpToLetter();

	MenuComponent mMenu;

	typedef OptionListComponent<char> LetterList;
	std::shared_ptr<LetterList> mJumpToLetterList;

	typedef OptionListComponent<const FileData::SortType*> SortList;
	std::shared_ptr<SortList> mListSort;

#if defined(EXTENSION)
	std::vector<std::function<void()>> mSaveFuncs;

	std::shared_ptr<SwitchComponent> mFavoriteOption;
	std::shared_ptr<SwitchComponent> mShowHidden;

	bool mFavoriteState;
	bool mHiddenState;
#endif
	SystemData* mSystem;
	IGameListView* getGamelist();
};
