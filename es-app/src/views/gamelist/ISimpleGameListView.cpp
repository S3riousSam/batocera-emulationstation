#include "views/gamelist/ISimpleGameListView.h"
#include "Settings.h"
#include "Sound.h"
#include "ThemeData.h"
#include "Window.h"
#include "views/ViewController.h"
#if defined(EXTENSION) || !defined(EXTENSION) // TODO: REVIEW
#include "Gamelist.h"
#include "LocaleES.h"
#include "Log.h"
#include "SystemData.h"
#endif

ISimpleGameListView::ISimpleGameListView(Window* window, FileData* root)
	: IGameListView(window, root)
	, mHeaderText(window)
	, mHeaderImage(window)
	, mBackground(window)
	, mThemeExtras(window)
#if defined(EXTENSION)
	, mFavoriteChange(false)
#endif
{
	mHeaderText.setText("Logo Text");
	mHeaderText.setSize(mSize.x(), 0);
	mHeaderText.setPosition(0, 0);
	mHeaderText.setAlignment(ALIGN_CENTER);

	mHeaderImage.setResize(0, mSize.y() * 0.185f);
	mHeaderImage.setOrigin(0.5f, 0.0f);
	mHeaderImage.setPosition(mSize.x() / 2, 0);

	mBackground.setResize(mSize.x(), mSize.y());

	addChild(&mHeaderText);
	addChild(&mBackground);
	addChild(&mThemeExtras);
}

void ISimpleGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	mBackground.applyTheme(theme, getName(), "background", ThemeFlags::ALL);
	mHeaderImage.applyTheme(theme, getName(), "logo", ThemeFlags::ALL);
	mHeaderText.applyTheme(theme, getName(), "logoText", ThemeFlags::ALL);
	mThemeExtras.setExtras(ThemeData::makeExtras(theme, getName(), mWindow));

	if (mHeaderImage.hasImage())
	{
		removeChild(&mHeaderText);
		addChild(&mHeaderImage);
	}
	else
	{
		addChild(&mHeaderText);
		removeChild(&mHeaderImage);
	}
}

void ISimpleGameListView::onFileChanged(FileData* file, FileChangeType change)
{
	// we could be tricky here to be efficient;
	// but this shouldn't happen very often so we'll just always repopulate
	FileData* cursor = getCursor();
	const int index = getCursorIndex();
	populateList(getRoot()->getChildren());
	setCursorIndex(index);
#if defined(EXTENSION)
	if (file->getType() == GAME)
	{
		const SystemData* favoriteSystem = SystemData::getFavoriteSystem();
		const bool isFavorite = (file->metadata.get("favorite") == "true");
		bool foundInFavorite = false;
		// Removing favorite case:
		for (const auto& gameInFavorite : favoriteSystem->getRootFolder()->getChildren())
		{
			if (gameInFavorite == file)
			{
				if (!isFavorite)
				{
					favoriteSystem->getRootFolder()->removeAlreadyExisitingChild(file);
					ViewController::get()->setInvalidGamesList(SystemData::getFavoriteSystem());
					ViewController::get()->getSystemListView()->manageFavorite();
				}
				foundInFavorite = true;
				break;
			}
		}
		// Adding favorite case:
		if (!foundInFavorite && isFavorite)
		{
			favoriteSystem->getRootFolder()->addAlreadyExisitingChild(file);
			ViewController::get()->setInvalidGamesList(SystemData::getFavoriteSystem());
			ViewController::get()->getSystemListView()->manageFavorite();
		}
	}
#endif
}

bool ISimpleGameListView::input(InputConfig* config, Input input)
{
	if (input.value != 0)
	{
		if (config->isMappedTo("b", input))
		{
			FileData* cursor = getCursor();
			if (cursor->getType() == GAME)
			{
#if !defined(EXTENSION)
				Sound::getFromTheme(getTheme(), getName(), "launch")->play();
#endif
				launch(cursor);
			}
			else
			{
				// it's a folder
				if (cursor->getChildren().size() > 0)
				{
					mCursorStack.push(cursor);
					populateList(cursor->getChildren());
				}
			}

			return true;
		}
		else if (config->isMappedTo("a", input))
		{
			if (mCursorStack.size())
			{
				populateList(getRoot()->getChildren());
				setCursor(mCursorStack.top());
				mCursorStack.pop();
#if !defined(EXTENSION)
				Sound::getFromTheme(getTheme(), getName(), "back")->play();
#endif
			}
			else
			{
				onFocusLost();
#if defined(EXTENSION)
				if (mFavoriteChange)
				{
					ViewController::get()->setInvalidGamesList(getRoot()->getSystem());
					mFavoriteChange = false;
				}

				ViewController::get()->goToSystemView(getRoot()->getSystem());
#else
				ViewController::get()->goToSystemView(getCursor()->getSystem());
#endif
			}

			return true;
		}
#if defined(EXTENSION)
		else if (config->isMappedTo("y", input))
		{
			FileData* cursor = getCursor();
			if (!ViewController::get()->getState().getSystem()->isFavorite() && cursor->getSystem()->getHasFavorites())
			{
				if (cursor->getType() == GAME)
				{
					mFavoriteChange = true;
					MetaDataList& md = cursor->metadata;
					bool removeFavorite = false;
					SystemData* favoriteSystem = SystemData::getFavoriteSystem();
					if (md.get("favorite") == "false")
					{
						md.set("favorite", "true");
						if (favoriteSystem != NULL)
							favoriteSystem->getRootFolder()->addAlreadyExisitingChild(cursor);
					}
					else
					{
						md.set("favorite", "false");
						if (favoriteSystem != NULL)
							favoriteSystem->getRootFolder()->removeAlreadyExisitingChild(cursor);
						removeFavorite = true;
					}
					if (favoriteSystem != NULL)
					{
						ViewController::get()->setInvalidGamesList(favoriteSystem);
						ViewController::get()->getSystemListView()->manageFavorite();
					}
					const int cursorPlace = getCursorIndex();
					populateList(cursor->getParent()->getChildren());
					setCursorIndex(cursorPlace + (removeFavorite ? -1 : 1));
					updateInfoPanel();
				}
			}
		}
#endif
		else if (config->isMappedTo("right", input))
		{
			if (Settings::getInstance()->getBool("QuickSystemSelect"))
			{
				onFocusLost();
#if defined(EXTENSION)
				if (mFavoriteChange)
				{
					ViewController::get()->setInvalidGamesList(getCursor()->getSystem());
					mFavoriteChange = false;
				}
#endif
				ViewController::get()->goToNextGameList();
				return true;
			}
		}
		else if (config->isMappedTo("left", input))
		{
			if (Settings::getInstance()->getBool("QuickSystemSelect"))
			{
				onFocusLost();
#if defined(EXTENSION)
				if (mFavoriteChange)
				{
					ViewController::get()->setInvalidGamesList(getCursor()->getSystem());
					mFavoriteChange = false;
				}
#endif
				ViewController::get()->goToPrevGameList();
				return true;
			}
		}
	}

	return IGameListView::input(config, input);
}
