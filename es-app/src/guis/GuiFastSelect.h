#pragma once
#include "GuiComponent.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include "views/gamelist/IGameListView.h"

class GuiFastSelect : public GuiComponent
{
public:
	GuiFastSelect(Window* window, IGameListView* gamelist);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;

private:
	void setScrollDir(int dir);
	void scroll();
	void updateGameListCursor();
	void updateGameListSort();
	void updateSortText();

	int mSortId;
	int mLetterId;

	int mScrollDir;
	int mScrollAccumulator;

	NinePatchComponent mBackground;
	TextComponent mSortText;
	TextComponent mLetterText;

	IGameListView* mGameList;
};
