#if defined(GRID_GAME_LIST_VIEW)
#pragma once
#include "components/ImageComponent.h"
#include "components/ImageGridComponent.h"
#include "views/gamelist/ISimpleGameListView.h"

class GridGameListView : public ISimpleGameListView
{
public:
	GridGameListView(Window* window, FileData* root);

	virtual FileData* getCursor() override;
	virtual void setCursor(FileData*) override;

	virtual bool input(InputConfig* config, Input input) override;

	virtual const char* getName() const override
	{
		return "grid";
	}

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

protected:
	virtual void populateList(const std::vector<FileData*>& files) override;
	virtual void launch(FileData* game) override;

	ImageGridComponent<FileData*> mGrid;
};
#endif