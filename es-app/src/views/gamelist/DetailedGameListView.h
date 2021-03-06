#pragma once
#include "components/DateTimeComponent.h"
#include "components/RatingComponent.h"
#include "components/ScrollableContainer.h"
#include "views/gamelist/BasicGameListView.h"

class SystemData;

class DetailedGameListView : public BasicGameListView
{
public:
	DetailedGameListView(Window* window, FileData* root, SystemData* system);

	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme) override;

	virtual const char* getName() const override
	{
		return "detailed";
	}

protected:
	virtual void launch(FileData* game) override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override; // EXTENSION

public: // private: // EXTENSION
	virtual void updateInfoPanel() override;

private:
	void initMDLabels();
	void initMDValues();

	ImageComponent mImage;

	TextComponent mLblRating, mLblReleaseDate, mLblDeveloper, mLblPublisher, mLblGenre, mLblPlayers, mLblLastPlayed, mLblPlayCount;
	TextComponent mLblFavorite; // EXTENSION

	RatingComponent mRating;
	DateTimeComponent mReleaseDate;
	TextComponent mDeveloper;
	TextComponent mPublisher;
	TextComponent mGenre;
	TextComponent mPlayers;
	DateTimeComponent mLastPlayed;
	TextComponent mPlayCount;
	TextComponent mFavorite; // EXTENSION

	std::vector<TextComponent*> getMDLabels();
	std::vector<GuiComponent*> getMDValues();

	ScrollableContainer mDescContainer;
	TextComponent mDescription;

	SystemData* mSystem; // EXTENSION
};
