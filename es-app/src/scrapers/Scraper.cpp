#include "scrapers/Scraper.h"
#include "Log.h"
#include "Settings.h"
#include <FreeImage.h>
#include <boost/filesystem.hpp>

#include "GamesDBScraper.h"
#if defined(EXTENSION)
#include "MamedbScraper.h"
#else
//#include "TheArchiveScraper.h"
#endif
#include "ScreenscraperScraper.h"

const std::map<std::string, generate_scraper_requests_func> scraper_request_funcs =
{
    { "TheGamesDB", &thegamesdb_generate_scraper_requests},
#if defined(EXTENSION)
#if defined(ENABLE_MAMEDB_SCRAPER)
    { "Mamedb", &mamedb_generate_scraper_requests},
#endif
    { "Screenscraper", &screenscraper_generate_scraper_requests}
#else
    { "TheArchive", &thearchive_generate_scraper_requests}
#endif
};

std::unique_ptr<ScraperSearchHandle> startScraperSearch(const ScraperSearchParams& params)
{
	const std::string& name = Settings::getInstance()->getString("Scraper");

	std::unique_ptr<ScraperSearchHandle> handle(new ScraperSearchHandle());
	scraper_request_funcs.at(name)(params, handle->mRequestQueue, handle->mResults);
	return handle;
}

std::vector<std::string> Scraper::getScraperList()
{
	std::vector<std::string> list;
	for (auto item : scraper_request_funcs)
		list.push_back(item.first);
	return list;
}

// ScraperSearchHandle
ScraperSearchHandle::ScraperSearchHandle()
{
	setStatus(AsyncHandleStatus::Progressing);
}

void ScraperSearchHandle::update()
{
	if (mStatus == AsyncHandleStatus::Done)
		return;

	while (!mRequestQueue.empty())
	{
		auto& req = mRequestQueue.front();
		AsyncHandleStatus status = req->status();

		if (status == AsyncHandleStatus::Error)
		{
			// propagate error
			setError(req->getStatusString());

			// empty our queue
			while (!mRequestQueue.empty())
				mRequestQueue.pop();

			return;
		}

		// finished this one, see if we have any more
		if (status == AsyncHandleStatus::Done)
		{
			mRequestQueue.pop();
			continue;
		}

		// status == ASYNC_IN_PROGRESS
	}

	// we finished without any errors!
	if (mRequestQueue.empty())
	{
		setStatus(AsyncHandleStatus::Done);
		return;
	}
}

// ScraperRequest
ScraperRequest::ScraperRequest(std::vector<ScraperSearchResult>& resultsWrite)
	: mResults(resultsWrite)
{
}

// ScraperHttpRequest
ScraperHttpRequest::ScraperHttpRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url)
	: ScraperRequest(resultsWrite)
	, mReq(std::unique_ptr<HttpReq>(new HttpReq(url)))
{
	setStatus(AsyncHandleStatus::Progressing);
}

void ScraperHttpRequest::update()
{
	const HttpReq::Status status = mReq->status();
	if (status == HttpReq::Status::Processing)
		return;

	if (status == HttpReq::Status::Success)
	{
		setStatus(AsyncHandleStatus::Done); // if process() has an error, status will be changed to ASYNC_ERROR
		process(mReq, mResults);
		return;
	}

	// everything else is some sort of error
	LOG(LogError) << "ScraperHttpRequest network error (status: " << static_cast<int>(status) << ") - " << mReq->getErrorMsg();
	setError(mReq->getErrorMsg());
}

// metadata resolving stuff

std::unique_ptr<MDResolveHandle> resolveMetaDataAssets(const ScraperSearchResult& result, const ScraperSearchParams& search)
{
	return std::unique_ptr<MDResolveHandle>(new MDResolveHandle(result, search));
}

MDResolveHandle::MDResolveHandle(const ScraperSearchResult& result, const ScraperSearchParams& search)
	: mResult(result)
{
	if (!result.imageUrl.empty())
	{
		const std::string imgPath = getSaveAsPath(search, "image", result.imageUrl);
		mFuncs.push_back(ResolvePair(downloadImageAsync(result.imageUrl, imgPath), [this, imgPath] {
			mResult.mdl.set("image", imgPath);
			mResult.imageUrl = std::string();
		}));
	}
}

void MDResolveHandle::update()
{
	if (mStatus == AsyncHandleStatus::Done || mStatus == AsyncHandleStatus::Error)
		return;

	auto it = mFuncs.begin();
	while (it != mFuncs.end())
	{
		if (it->first->status() == AsyncHandleStatus::Error)
		{
			setError(it->first->getStatusString());
			return;
		}
		else if (it->first->status() == AsyncHandleStatus::Done)
		{
			it->second();
			it = mFuncs.erase(it);
			continue;
		}
		it++;
	}

	if (mFuncs.empty())
		setStatus(AsyncHandleStatus::Done);
}

std::unique_ptr<ImageDownloadHandle> downloadImageAsync(const std::string& url, const std::string& saveAs)
{
	return std::unique_ptr<ImageDownloadHandle>(new ImageDownloadHandle(
		url, saveAs, Settings::getInstance()->getInt("ScraperResizeWidth"), Settings::getInstance()->getInt("ScraperResizeHeight")));
}

ImageDownloadHandle::ImageDownloadHandle(const std::string& url, const std::string& path, int maxWidth, int maxHeight)
	: mSavePath(path)
	, mMaxWidth(maxWidth)
	, mMaxHeight(maxHeight)
	, mReq(new HttpReq(url))
{
}

void ImageDownloadHandle::update()
{
	if (mReq->status() == HttpReq::Status::Processing)
		return;

	if (mReq->status() != HttpReq::Status::Success)
	{
		std::stringstream ss;
		ss << "Network error: " << mReq->getErrorMsg();
		setError(ss.str());
		return;
	}

	// download is done, save it to disk
	std::ofstream stream(mSavePath, std::ios_base::out | std::ios_base::binary);
	if (stream.bad())
	{
		setError("Failed to open image path to write. Permission error? Disk full?");
		return;
	}

	const std::string& content = mReq->getContent();
	stream.write(content.data(), content.length());
	stream.close();
	if (stream.bad())
	{
		setError("Failed to save image. Disk full?");
		return;
	}

	// resize it
	if (!resizeImage(mSavePath, mMaxWidth, mMaxHeight))
	{
		setError("Error saving resized image. Out of memory? Disk full?");
		return;
	}

	setStatus(AsyncHandleStatus::Done);
}

// you can pass 0 for width or height to keep aspect ratio
bool resizeImage(const std::string& path, int maxWidth, int maxHeight)
{
	// nothing to do
	if (maxWidth == 0 && maxHeight == 0)
		return true;

	FIBITMAP* image = NULL;

	// detect the filetype
	FREE_IMAGE_FORMAT format = FreeImage_GetFileType(path.c_str(), 0);
	if (format == FIF_UNKNOWN)
		format = FreeImage_GetFIFFromFilename(path.c_str());
	if (format == FIF_UNKNOWN)
	{
		LOG(LogError) << "Error - could not detect filetype for image \"" << path << "\"!";
		return false;
	}

	// make sure we can read this filetype first, then load it
	if (FreeImage_FIFSupportsReading(format))
	{
		image = FreeImage_Load(format, path.c_str());
	}
	else
	{
		LOG(LogError) << "Error - file format reading not supported for image \"" << path << "\"!";
		return false;
	}

	const float width = (float)FreeImage_GetWidth(image);
	const float height = (float)FreeImage_GetHeight(image);

	if (maxWidth == 0)
	{
		maxWidth = (int)((maxHeight / height) * width);
	}
	else if (maxHeight == 0)
	{
		maxHeight = (int)((maxWidth / width) * height);
	}

	FIBITMAP* imageRescaled = FreeImage_Rescale(image, maxWidth, maxHeight, FILTER_BILINEAR);
	FreeImage_Unload(image);

	if (imageRescaled == NULL)
	{
		LOG(LogError) << "Could not resize image! (not enough memory? invalid bitdepth?)";
		return false;
	}

	const bool saved = (FreeImage_Save(format, imageRescaled, path.c_str()) != FALSE);
	FreeImage_Unload(imageRescaled);

	if (!saved)
		LOG(LogError) << "Failed to save resized image!";

	return saved;
}

std::string getSaveAsPath(const ScraperSearchParams& params, const std::string& suffix, const std::string& url)
{
	const std::string& subdirectory = params.system->getName();
	const std::string name = params.game->getPath().stem().generic_string() + "-" + suffix;

#if !defined(EXTENSION)
	std::string path = getHomePath() + "/.emulationstation/downloaded_images/";
#else
	// default dir in rom directory
	std::string path = params.system->getRootFolder()->getPath().generic_string() + "/downloaded_images/";
	if (!boost::filesystem::exists(path) && !boost::filesystem::create_directory(path))
	{
		// Unable to create the directory in system rom dir, fallback on ~
		path = getHomePath() + "/.emulationstation/downloaded_images/" + subdirectory + "/";
	}
#endif

	if (!boost::filesystem::exists(path))
#if !defined(EXTENSION)
		boost::filesystem::create_directory(path);
#else
		boost::filesystem::create_directories(path);
#endif

#if !defined(EXTENSION)
	path += subdirectory + "/";

	if (!boost::filesystem::exists(path))
		boost::filesystem::create_directory(path);
#endif
	const size_t dot = url.find_last_of('.');
	std::string ext;
	if (dot != std::string::npos)
		ext = url.substr(dot, std::string::npos);

	return path + name + ext;
}
