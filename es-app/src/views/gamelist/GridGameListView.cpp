#if defined(GRID_GAME_LIST_VIEW)
#include "views/gamelist/GridGameListView.h"
#include "LocaleES.h"
#if defined(EXTENSION)
#include "Settings.h"
#endif
#include "ThemeData.h"
#include "Window.h"
#include "views/ViewController.h"

GridGameListView::GridGameListView(Window* window, FileData* root)
	: ISimpleGameListView(window, root)
	, mGrid(window)
{
	mGrid.setPosition(0, mSize.y() * 0.2f);
	mGrid.setSize(mSize.x(), mSize.y() * 0.8f);
	addChild(&mGrid);

	populateList(root->getChildren());
}

FileData* GridGameListView::getCursor()
{
	return mGrid.getSelected();
}

void GridGameListView::setCursor(FileData* file)
{
	if (!mGrid.setCursor(file))
	{
		populateList(file->getParent()->getChildren());
		mGrid.setCursor(file);
	}
}

bool GridGameListView::input(InputConfig* config, Input input)
{
	if (config->isMappedTo("left", input) || config->isMappedTo("right", input))
		return GuiComponent::input(config, input);

	return ISimpleGameListView::input(config, input);
}

void GridGameListView::populateList(const std::vector<FileData*>& files)
{
	mGrid.clear();
	for (auto it = files.begin(); it != files.end(); it++)
	{
#if defined(EXTENSION)
		if (Settings::getInstance()->getBool("FavoritesOnly"))
		{
			if ((*it)->metadata.get("favorite").compare("true") == 0)
			{
				mGrid.add((*it)->getName(), (*it)->getThumbnailPath(), *it);
			}
		}
		else
#endif
		{
			mGrid.add((*it)->getName(), (*it)->getThumbnailPath(), *it);
		}
	}
}

void GridGameListView::launch(FileData* game)
{
	ViewController::get()->launch(game);
}

std::vector<HelpPrompt> GridGameListView::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("up/down/left/right", _("SCROLL")));
	prompts.push_back(HelpPrompt(BUTTON_LAUNCH, _("LAUNCH")));
	prompts.push_back(HelpPrompt(BUTTON_BACK, _("BACK")));
	return prompts;
}
#endif