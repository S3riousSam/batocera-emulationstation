#if defined(MANUAL_SCRAPING)
#include "components/ScraperSearchComponent.h"
#include "FileData.h"
#include "HttpReq.h"
#include "LocaleES.h"
#include "Log.h"
#include "Settings.h"
#include "Util.h"
#include "Window.h"
#include "components/AnimatedImageComponent.h"
#include "components/ComponentList.h"
#include "components/DateTimeComponent.h"
#include "components/ImageComponent.h"
#include "components/RatingComponent.h"
#include "components/ScrollableContainer.h"
#include "components/TextComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiTextEditPopup.h"
#if defined(EXTENSION)
#include "guis/GuiTextEditPopupKeyboard.h"
#endif

ScraperSearchComponent::ScraperSearchComponent(Window* window, SearchType type)
	: GuiComponent(window)
	, mGrid(window, Eigen::Vector2i(4, 3))
	, mBusyAnim(window)
	, mSearchType(type)
	, mBlockAccept(false)
{
	addChild(&mGrid);

	// left spacer (empty component, needed for borders)
	mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Eigen::Vector2i(0, 0), false, false, Eigen::Vector2i(1, 3),
		GridFlags::BORDER_TOP | GridFlags::BORDER_BOTTOM);

	// selected result name
	mResultName = std::make_shared<TextComponent>(mWindow, "RESULT NAME", Font::get(FONT_SIZE_MEDIUM), COLOR_GRAY3);

	// selected result thumbnail
	mResultThumbnail = std::make_shared<ImageComponent>(mWindow);
	mGrid.setEntry(mResultThumbnail, Eigen::Vector2i(1, 1), false, false, Eigen::Vector2i(1, 1));

	// selected result desc + container
	mDescContainer = std::make_shared<ScrollableContainer>(mWindow);
	mResultDesc = std::make_shared<TextComponent>(mWindow, "RESULT DESC", Font::get(FONT_SIZE_SMALL), COLOR_GRAY3);
	mDescContainer->addChild(mResultDesc.get());
	mDescContainer->setAutoScroll(true);

	// metadata
	const auto font = Font::get(FONT_SIZE_SMALL); // this gets replaced in onSizeChanged() so its just a placeholder
	const unsigned int COLOR_TEXT = COLOR_GRAY3;
	const unsigned int COLOR_LABEL = COLOR_GRAYX;
	mMD_Rating = std::make_shared<RatingComponent>(mWindow);
	mMD_ReleaseDate = std::make_shared<DateTimeComponent>(mWindow);
	mMD_ReleaseDate->setColor(COLOR_TEXT);
	mMD_Developer = std::make_shared<TextComponent>(mWindow, "", font, COLOR_TEXT);
	mMD_Publisher = std::make_shared<TextComponent>(mWindow, "", font, COLOR_TEXT);
	mMD_Genre = std::make_shared<TextComponent>(mWindow, "", font, COLOR_TEXT);
	mMD_Players = std::make_shared<TextComponent>(mWindow, "", font, COLOR_TEXT);

	const static std::string column(":");
	mMD_Pairs = {
		MetaDataPair(std::make_shared<TextComponent>(mWindow, strToUpper(_("Rating") + column), font, COLOR_LABEL), mMD_Rating, false),
		MetaDataPair(std::make_shared<TextComponent>(mWindow, strToUpper(_("Released") + column), font, COLOR_LABEL), mMD_ReleaseDate),
		MetaDataPair(std::make_shared<TextComponent>(mWindow, strToUpper(_("Developer") + column), font, COLOR_LABEL), mMD_Developer),
		MetaDataPair(std::make_shared<TextComponent>(mWindow, strToUpper(_("Publisher") + column), font, COLOR_LABEL), mMD_Publisher),
		MetaDataPair(std::make_shared<TextComponent>(mWindow, strToUpper(_("Genre") + column), font, COLOR_LABEL), mMD_Genre),
		MetaDataPair(std::make_shared<TextComponent>(mWindow, strToUpper(_("Players") + column), font, COLOR_LABEL), mMD_Players)
	};

	mMD_Grid = std::make_shared<ComponentGrid>(mWindow, Eigen::Vector2i(2, mMD_Pairs.size() * 2 - 1));
	unsigned int index = 0;
	for (auto& it : mMD_Pairs)
	{
		mMD_Grid->setEntry(it.first, Eigen::Vector2i(0, index), false, true);
		mMD_Grid->setEntry(it.second, Eigen::Vector2i(1, index), false, it.resize);
		index += 2;
	}

	mGrid.setEntry(mMD_Grid, Eigen::Vector2i(2, 1), false, false);

	// result list
	mResultList = std::make_shared<ComponentList>(mWindow);
	mResultList->setCursorChangedCallback([this](CursorState state) {
		if (state == CURSOR_STOPPED)
			updateInfoPane();
	});

	updateViewStyle();
}

void ScraperSearchComponent::onSizeChanged()
{
	mGrid.setSize(mSize);

	if (mSize.x() == 0 || mSize.y() == 0)
		return;

	// column widths
	if (mSearchType == ALWAYS_ACCEPT_FIRST_RESULT)
		mGrid.setColWidthPerc(0, 0.02f); // looks better when this is higher in auto mode
	else
		mGrid.setColWidthPerc(0, 0.01f);

	mGrid.setColWidthPerc(1, 0.25f);
	mGrid.setColWidthPerc(2, 0.25f);

	// row heights
	if (mSearchType == ALWAYS_ACCEPT_FIRST_RESULT) // show name
		mGrid.setRowHeightPerc(0, (mResultName->getFont()->getHeight() * 1.6f) / mGrid.getSize().y()); // result name
	else
		mGrid.setRowHeightPerc(0, 0.0825f); // hide name but do padding

	if (mSearchType == ALWAYS_ACCEPT_FIRST_RESULT)
		mGrid.setRowHeightPerc(2, 0.2f);
	else
		mGrid.setRowHeightPerc(1, 0.505f);

	const float boxartCellScale = 0.9f;

	// limit thumbnail size using setMaxHeight - we do this instead of letting mGrid call setSize because it maintains the aspect ratio
	// we also pad a little so it doesn't rub up against the metadata labels
	mResultThumbnail->setMaxSize(mGrid.getColWidth(1) * boxartCellScale, mGrid.getRowHeight(1));

	// metadata
	resizeMetadata();

	if (mSearchType != ALWAYS_ACCEPT_FIRST_RESULT)
		mDescContainer->setSize(mGrid.getColWidth(1) * boxartCellScale + mGrid.getColWidth(2), mResultDesc->getFont()->getHeight() * 3);
	else
		mDescContainer->setSize(mGrid.getColWidth(3) * boxartCellScale, mResultDesc->getFont()->getHeight() * 8);

	mResultDesc->setSize(mDescContainer->getSize().x(), 0); // make desc text wrap at edge of container

	mGrid.onSizeChanged();

	mBusyAnim.setSize(mSize);
}

void ScraperSearchComponent::resizeMetadata()
{
	mMD_Grid->setSize(mGrid.getColWidth(2), mGrid.getRowHeight(1));
	if (mMD_Grid->getSize().y() > mMD_Pairs.size())
	{
		const int fontHeight = (int)(mMD_Grid->getSize().y() / mMD_Pairs.size() * 0.8f);
		const auto fontLbl = Font::get(fontHeight, FONT_PATH_REGULAR);
		const auto fontComp = Font::get(fontHeight, FONT_PATH_LIGHT);

		// update label fonts
		float maxLblWidth = 0;
		for (auto& it : mMD_Pairs)
		{
			it.first->setFont(fontLbl);
			it.first->setSize(0, 0);
			if (it.first->getSize().x() > maxLblWidth)
				maxLblWidth = it.first->getSize().x() + 6;
		}

		for (unsigned int i = 0; i < mMD_Pairs.size(); i++)
		{
			mMD_Grid->setRowHeightPerc(i * 2, (fontLbl->getLetterHeight() + 2) / mMD_Grid->getSize().y());
		}

		// update component fonts
		mMD_ReleaseDate->setFont(fontComp);
		mMD_Developer->setFont(fontComp);
		mMD_Publisher->setFont(fontComp);
		mMD_Genre->setFont(fontComp);
		mMD_Players->setFont(fontComp);

		mMD_Grid->setColWidthPerc(0, maxLblWidth / mMD_Grid->getSize().x());

		// rating is manually sized
		mMD_Rating->setSize(mMD_Grid->getColWidth(1), fontLbl->getHeight() * 0.65f);
		mMD_Grid->onSizeChanged();

		// make result font follow label font
		mResultDesc->setFont(Font::get(fontHeight, FONT_PATH_REGULAR));
	}
}

void ScraperSearchComponent::updateViewStyle()
{
	using Eigen::Vector2i;

	// unlink description and result list and result name
	mGrid.removeEntry(mResultName);
	mGrid.removeEntry(mResultDesc);
	mGrid.removeEntry(mResultList);

	// add them back depending on search type
	if (mSearchType == ALWAYS_ACCEPT_FIRST_RESULT)
	{
		// show name
		mGrid.setEntry(mResultName, Vector2i(1, 0), false, true, Vector2i(2, 1), GridFlags::BORDER_TOP);

		// need a border on the bottom left
		mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Vector2i(0, 2), false, false, Vector2i(3, 1), GridFlags::BORDER_BOTTOM);

		// show description on the right
		mGrid.setEntry(mDescContainer, Vector2i(3, 0), false, false, Vector2i(1, 3), GridFlags::BORDER_TOP | GridFlags::BORDER_BOTTOM);
		mResultDesc->setSize(mDescContainer->getSize().x(), 0); // make desc text wrap at edge of container
	}
	else
	{
		// fake row where name would be
		mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Vector2i(1, 0), false, true, Vector2i(2, 1), GridFlags::BORDER_TOP);

		// show result list on the right
		mGrid.setEntry(mResultList, Vector2i(3, 0), true, true, Vector2i(1, 3), GridFlags::BORDER_LEFT | GridFlags::BORDER_TOP | GridFlags::BORDER_BOTTOM);

		// show description under image/info
		mGrid.setEntry(mDescContainer, Vector2i(1, 2), false, false, Vector2i(2, 1), GridFlags::BORDER_BOTTOM);
		mResultDesc->setSize(mDescContainer->getSize().x(), 0); // make desc text wrap at edge of container
	}
}

void ScraperSearchComponent::search(const ScraperSearchParams& params)
{
	mResultList->clear();
	mScraperResults.clear();
	mThumbnailReq.reset();
	mMDResolveHandle.reset();
	updateInfoPane();

	mLastSearch = params;
	mSearchHandle = Scraper::startSearch(params);
}

void ScraperSearchComponent::stop()
{
	mThumbnailReq.reset();
	mSearchHandle.reset();
	mMDResolveHandle.reset();
	mBlockAccept = false;
}

void ScraperSearchComponent::onSearchDone(const std::vector<ScraperSearchResult>& results)
{
	mResultList->clear();

	mScraperResults = results;

	const int end = results.size() > ScraperHttpRequest::MAX_SCRAPER_RESULTS ? ScraperHttpRequest::MAX_SCRAPER_RESULTS : results.size(); // at max display 5

	const auto font = Font::get(FONT_SIZE_MEDIUM);

	if (end == 0)
	{
		ComponentListRow row;
		row.addElement(std::make_shared<TextComponent>(mWindow, _("NO GAMES FOUND - SKIP"), font, COLOR_GRAY3), true);

		if (mSkipCallback)
			row.makeAcceptInputHandler(mSkipCallback);

		mResultList->addRow(row);
		mGrid.resetCursor();
	}
	else
	{
		ComponentListRow row;
		for (int i = 0; i < end; i++)
		{
			row.elements.clear();
			row.addElement(std::make_shared<TextComponent>(mWindow, strToUpper(results.at(i).mdl.get("name")), font, COLOR_GRAY3), true);
			row.makeAcceptInputHandler([this, i] { returnResult(mScraperResults.at(i)); });
			mResultList->addRow(row);
		}
		mGrid.resetCursor();
	}

	mBlockAccept = false;
	updateInfoPane();

	if (mSearchType == ALWAYS_ACCEPT_FIRST_RESULT)
	{
		if (mScraperResults.size() == 0)
			mSkipCallback();
		else
			returnResult(mScraperResults.front());
	}
#if defined(ENABLE_SCRAPER_CRC)
	else if (mSearchType == ALWAYS_ACCEPT_MATCHING_CRC)
	{
		// TODO
	}
#endif
}

void ScraperSearchComponent::onSearchError(const std::string& error)
{
	LOG(LogInfo) << "ScraperSearchComponent search error: " << error;
	mWindow->pushGui(new GuiMsgBox(mWindow, strToUpper(error), _("RETRY"), std::bind(&ScraperSearchComponent::search, this, mLastSearch), _("SKIP"),
		mSkipCallback, _("CANCEL"), mCancelCallback));
}

int ScraperSearchComponent::getSelectedIndex() const
{
	return (!mScraperResults.size() || mGrid.getSelectedComponent() != mResultList)  ? -1 : mResultList->getCursorId();
}

void ScraperSearchComponent::updateInfoPane()
{
	int index = getSelectedIndex();
	if (mSearchType == ALWAYS_ACCEPT_FIRST_RESULT && mScraperResults.size())
		index = 0;

	if (index != -1 && static_cast<int>(mScraperResults.size()) > index)
	{
		const ScraperSearchResult& res = mScraperResults.at(index);
		mResultName->setText(strToUpper(res.mdl.get("name")));
		mResultDesc->setText(strToUpper(res.mdl.get("desc")));
		mDescContainer->reset();

		mResultThumbnail->setImage("");
		const std::string& thumb = res.thumbnailUrl.empty() ? res.imageUrl : res.thumbnailUrl;
		if (!thumb.empty())
			mThumbnailReq = std::unique_ptr<HttpReq>(new HttpReq(thumb));
		else
			mThumbnailReq.reset();

		// metadata
		mMD_Rating->setValue(strToUpper(res.mdl.get("rating")));
		mMD_ReleaseDate->setValue(strToUpper(res.mdl.get("releasedate")));
		mMD_Developer->setText(strToUpper(res.mdl.get("developer")));
		mMD_Publisher->setText(strToUpper(res.mdl.get("publisher")));
		mMD_Genre->setText(strToUpper(res.mdl.get("genre")));
		mMD_Players->setText(strToUpper(res.mdl.get("players")));
		mGrid.onSizeChanged();
	}
	else
	{
		mResultName->resetText();
		mResultDesc->resetText();
		mResultThumbnail->setImage(std::string());

		// metadata
		mMD_Rating->setValue(std::string());
		mMD_ReleaseDate->setValue(std::string());
		mMD_Developer->resetText();
		mMD_Publisher->resetText();
		mMD_Genre->resetText();
		mMD_Players->resetText();
	}
}

bool ScraperSearchComponent::input(InputConfig* config, Input input)
{
	if (config->isMappedTo("b", input) && input.value != 0)
	{
		if (mBlockAccept)
			return true;
	}

	return GuiComponent::input(config, input);
}

void ScraperSearchComponent::render(const Eigen::Affine3f& parentTrans)
{
	Eigen::Affine3f trans = parentTrans * getTransform();

	renderChildren(trans);

	if (mBlockAccept)
	{
		Renderer::setMatrix(trans);
		Renderer::drawRect(0.f, 0.f, mSize.x(), mSize.y(), 0x00000011);
		// Renderer::drawRect((int)mResultList->getPosition().x(), (int)mResultList->getPosition().y(),
		//	(int)mResultList->getSize().x(), (int)mResultList->getSize().y(), 0x00000011);

		mBusyAnim.render(trans);
	}
}

void ScraperSearchComponent::returnResult(ScraperSearchResult result)
{
	mBlockAccept = true;

	// resolve metadata image before returning
	if (!result.imageUrl.empty())
	{
		mMDResolveHandle = Scraper::resolveMetaDataAssets(result, mLastSearch);
		return;
	}

	mAcceptCallback(result);
}

void ScraperSearchComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mBlockAccept)
		mBusyAnim.update(deltaTime);

	if (mThumbnailReq && mThumbnailReq->status() != HttpReq::Status::Processing)
		updateThumbnail();

	if (mSearchHandle && mSearchHandle->status() != AsyncHandleStatus::Progressing)
	{
		auto status = mSearchHandle->status();
		auto results = mSearchHandle->getResults();
		auto statusString = mSearchHandle->getStatusString();

		// we reset here because onSearchDone in auto mode can call mSkipCallback() which can call
		// another search() which will set our mSearchHandle to something important
		mSearchHandle.reset();

		if (status == AsyncHandleStatus::Done)
			onSearchDone(results);
		else if (status == AsyncHandleStatus::Error)
			onSearchError(statusString);
	}

	if (mMDResolveHandle && mMDResolveHandle->status() != AsyncHandleStatus::Progressing)
	{
		if (mMDResolveHandle->status() == AsyncHandleStatus::Done)
		{
			ScraperSearchResult result = mMDResolveHandle->getResult();
			mMDResolveHandle.reset();

			// this might end in us being deleted, depending on mAcceptCallback - so make sure this is the last thing we do in update()
			returnResult(result);
		}
		else if (mMDResolveHandle->status() == AsyncHandleStatus::Error)
		{
			onSearchError(mMDResolveHandle->getStatusString());
			mMDResolveHandle.reset();
		}
	}
}

void ScraperSearchComponent::updateThumbnail()
{
	if (mThumbnailReq && mThumbnailReq->status() == HttpReq::Status::Success)
	{
		const std::string content = mThumbnailReq->getContent();
		mResultThumbnail->setImage(content.data(), content.length());
		mGrid.onSizeChanged(); // a hack to fix the thumbnail position since its size changed
	}
	else
	{
		LOG(LogWarning) << "Thumbnail request failed: " << mThumbnailReq->getErrorMsg();
		mResultThumbnail->resetImage();
	}

	mThumbnailReq.reset();
}

void ScraperSearchComponent::openInputScreen(ScraperSearchParams& params)
{
	auto searchForFunc = [&](const std::string& name) {
		params.nameOverride = name;
		search(params);
	};
	stop();

	// Initial value is last search if there was one, otherwise the clean path name
	const std::string initValue = params.nameOverride.empty() ? params.game->getCleanName() : params.nameOverride;
#if defined(EXTENSION)
	if (Settings::getInstance()->getBool("UseOSK"))
	{
		mWindow->pushGui(new GuiTextEditPopupKeyboard(mWindow, _("SEARCH FOR"), initValue, searchForFunc, false, _("SEARCH")));
	}
	else
#endif
	{
		mWindow->pushGui(new GuiTextEditPopup(mWindow, _("SEARCH FOR"), initValue, searchForFunc, false, _("SEARCH")));
	}
}

std::vector<HelpPrompt> ScraperSearchComponent::getHelpPrompts()
{
    if (getSelectedIndex() != -1)
    {
        std::vector<HelpPrompt> prompts = mGrid.getHelpPrompts();
        prompts.push_back(HelpPrompt("b", _("ACCEPT RESULT")));
        return prompts;
    }
    return mGrid.getHelpPrompts();
}

void ScraperSearchComponent::onFocusGained()
{
	mGrid.onFocusGained();
}

void ScraperSearchComponent::onFocusLost()
{
	mGrid.onFocusLost();
}
#endif