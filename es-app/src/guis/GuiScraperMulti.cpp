#if defined(MANUAL_SCRAPING)
#include "guis/GuiScraperMulti.h"
#include "Gamelist.h"
#include "LocaleES.h"
#include "Log.h"
#include "Renderer.h"
#include "SystemData.h"
#include "views/ViewController.h"
#include "components/ButtonComponent.h"
#include "components/MenuComponent.h" // for makeButtonGrid
#include "components/ScraperSearchComponent.h"
#include "components/TextComponent.h"
#include "guis/GuiMsgBox.h"

using namespace Eigen;

GuiScraperMulti::GuiScraperMulti(Window* window, const std::queue<ScraperSearchParams>& searches, bool approveResults)
	: GuiComponent(window)
	, mBackground(window, ":/frame.png")
	, mGrid(window, Vector2i(1, 5))
	, mSearchQueue(searches)
{
	assert(mSearchQueue.size());

	addChild(&mBackground);
	addChild(&mGrid);
#if defined(EXTENSION)
	mIsProcessing = true;
#endif
	mTotalGames = mSearchQueue.size();
	mCurrentGame = 0;
	mTotalSuccessful = 0;
	mTotalSkipped = 0;

	// set up grid
	mTitle = std::make_shared<TextComponent>(mWindow, _("SCRAPING IN PROGRESS"), Font::get(FONT_SIZE_LARGE), 0x555555FF, ALIGN_CENTER);
	mGrid.setEntry(mTitle, Vector2i(0, 0), false, true);

	mSystem = std::make_shared<TextComponent>(mWindow, _("SYSTEM"), Font::get(FONT_SIZE_MEDIUM), 0x777777FF, ALIGN_CENTER);
	mGrid.setEntry(mSystem, Vector2i(0, 1), false, true);

	mSubtitle = std::make_shared<TextComponent>(mWindow, _("subtitle text"), Font::get(FONT_SIZE_SMALL), 0x888888FF, ALIGN_CENTER);
	mGrid.setEntry(mSubtitle, Vector2i(0, 2), false, true);
#if defined(ENABLE_SCRAPER_CRC)
	mSearchComp = std::make_shared<ScraperSearchComponent>(mWindow, approveResults
		? ScraperSearchComponent::ALWAYS_ACCEPT_MATCHING_CRC
		: ScraperSearchComponent::ALWAYS_ACCEPT_FIRST_RESULT);
#else
	mSearchComp = std::make_shared<ScraperSearchComponent>(mWindow,
		ScraperSearchComponent::ALWAYS_ACCEPT_FIRST_RESULT);
#endif
	mSearchComp->setAcceptCallback(std::bind(&GuiScraperMulti::acceptResult, this, std::placeholders::_1));
	mSearchComp->setSkipCallback(std::bind(&GuiScraperMulti::skip, this));
	mSearchComp->setCancelCallback(std::bind(&GuiScraperMulti::finish, this));
	mGrid.setEntry(mSearchComp, Vector2i(0, 3), mSearchComp->getSearchType() != ScraperSearchComponent::ALWAYS_ACCEPT_FIRST_RESULT, true);

	std::vector<std::shared_ptr<ButtonComponent>> buttons;

	if (approveResults)
	{
		buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("INPUT"), _("search"), [&] {
			mSearchComp->openInputScreen(mSearchQueue.front());
			mGrid.resetCursor();
		}));

		buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("SKIP"), _("SKIP"), [&] {
			skip();
			mGrid.resetCursor();
		}));
	}

	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, _("STOP"), _("stop (progress saved)"), std::bind(&GuiScraperMulti::finish, this)));

	mButtonGrid = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 4), true, false);

	setSize(Renderer::getScreenWidth() * 0.95f, Renderer::getScreenHeight() * 0.849f);
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);

	doNextSearch();
}

GuiScraperMulti::~GuiScraperMulti()
{
	// view type probably changed (basic -> detailed)
	for (auto it = SystemData::sSystemVector.begin(); it != SystemData::sSystemVector.end(); it++)
		ViewController::get()->reloadGameListView(*it, false);
}

void GuiScraperMulti::onSizeChanged()
{
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mGrid.setRowHeightPerc(0, mTitle->getFont()->getLetterHeight() * 1.9725f / mSize.y(), false);
	mGrid.setRowHeightPerc(1, (mSystem->getFont()->getLetterHeight() + 2) / mSize.y(), false);
	mGrid.setRowHeightPerc(2, mSubtitle->getFont()->getHeight() * 1.75f / mSize.y(), false);
	mGrid.setRowHeightPerc(4, mButtonGrid->getSize().y() / mSize.y(), false);
	mGrid.setSize(mSize);
}

void GuiScraperMulti::doNextSearch()
{
	if (mSearchQueue.empty())
	{
		finish();
		return;
	}

	// update title
	std::stringstream ss;
	mSystem->setText(strToUpper(mSearchQueue.front().system->getFullName()));

	// update subtitle
	ss.str(""); // clear
#if defined(EXTENSION)
	char strbuf[256];
	snprintf(strbuf, 256, _("GAME %i OF %i").c_str(), mCurrentGame + 1, mTotalGames);

	ss << strbuf << " - " << strToUpper(mSearchQueue.front().game->getPath().filename().string());
#else
	ss << "GAME " << (mCurrentGame + 1) << " OF " << mTotalGames << " - " << strToUpper(mSearchQueue.front().game->getPath().filename().string());
#endif
	mSubtitle->setText(ss.str());

	mSearchComp->search(mSearchQueue.front());
}

void GuiScraperMulti::acceptResult(const ScraperSearchResult& result)
{
	ScraperSearchParams& search = mSearchQueue.front();
#if defined(EXTENSION)
	search.game->metadata.merge(result.mdl);
#else
	search.game->metadata = result.mdl;
#endif
	updateGamelist(search.system);

	mSearchQueue.pop();
	mCurrentGame++;
	mTotalSuccessful++;
	doNextSearch();
}

void GuiScraperMulti::skip()
{
	mSearchQueue.pop();
	mCurrentGame++;
	mTotalSkipped++;
	doNextSearch();
}

void GuiScraperMulti::finish()
{
	std::stringstream ss;
	if (mTotalSuccessful == 0)
	{
		ss << _("NO GAMES WERE SCRAPED.");
	}
	else
	{
		char strbuf[256];
		snprintf(
			strbuf, 256, boost::locale::ngettext("%i GAME SUCCESSFULLY SCRAPED!", "%i GAMES SUCCESSFULLY SCRAPED!", mTotalSuccessful).c_str(), mTotalSuccessful);
		ss << strbuf;

		if (mTotalSkipped > 0)
		{
			snprintf(strbuf, 256, boost::locale::ngettext("%i GAME SKIPPED.", "%i GAMES SKIPPED.", mTotalSkipped).c_str(), mTotalSkipped);
			ss << "\n" << strbuf;
		}
	}

	mWindow->pushGui(new GuiMsgBox(mWindow, ss.str(), _("OK"), [&] { delete this; }));
#if defined(EXTENSION)
	mIsProcessing = false;
#endif
}

std::vector<HelpPrompt> GuiScraperMulti::getHelpPrompts()
{
	return mGrid.getHelpPrompts();
}
#endif