#if defined(MANUAL_SCRAPING)
#pragma once
#include "AsyncHandle.h"
#include "HttpReq.h"
#include "MetaData.h"
#include "SystemData.h"
#include <functional>
#include <queue>
#include <vector>

struct ScraperSearchParams
{
	SystemData* system;
	FileData* game;
	std::string nameOverride; // TheGamesDBRequest-specific
};

struct ScraperSearchResult
{
	ScraperSearchResult()
		: mdl(GAME_METADATA)
	{
	}

	MetaDataList mdl;
	std::string imageUrl;
	std::string thumbnailUrl;
};

// So let me explain why I've abstracted this so heavily.
// There are two ways I can think of that you'd want to write a scraper.

// 1. Do some HTTP request(s) -> process it -> return the results
// 2. Do some local filesystem queries (an offline scraper) -> return the results

// The first way needs to be asynchronous while it's waiting for the HTTP request to return.
// The second doesn't.

// It would be nice if we could write it like this:
// search = generate_http_request(searchparams);
// wait_until_done(search);
// ... process search ...
// return results;

// We could do this if we used threads.  Right now ES doesn't because I'm pretty sure I'll fuck it up,
// and I'm not sure of the performance of threads on the Pi (single-core ARM).
// We could also do this if we used coroutines.
// I can't find a really good cross-platform coroutine library (x86/64/ARM Linux + Windows),
// and I don't want to spend more time chasing libraries than just writing it the long way once.

// So, I did it the "long" way.
// ScraperSearchHandle - one logical search, e.g. "search for mario"
// ScraperRequest - encapsulates some sort of asynchronous request that will ultimately return some results
// ScraperHttpRequest - implementation of ScraperRequest that waits on an HttpReq, then processes it with some processing function.

// Gathers results from (potentially multiple) ScraperRequests
class ScraperRequest : public AsyncHandle // Abstract class
{
};

// Defines a single HTTP request to process for results
class ScraperHttpRequest : public ScraperRequest // Abstract class
{
public:
	static const size_t MAX_SCRAPER_RESULTS = 7ull;

protected:
	ScraperHttpRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url);

private:
	virtual void process(const std::unique_ptr<HttpReq>& req, std::vector<ScraperSearchResult>& results) = 0;
	void update() override;

	std::unique_ptr<HttpReq> mReq;
	std::vector<ScraperSearchResult>& mResults;
};

// Defines a request to get a list of results
class ScraperSearchHandle final : public AsyncHandle
{
public:
	ScraperSearchHandle();

	const std::vector<ScraperSearchResult>& getResults() const
	{
		assert(mStatus != AsyncHandleStatus::Progressing);
		return mResults;
	}

private:
	void update() override;

	friend std::unique_ptr<ScraperSearchHandle> startScraperSearch(const ScraperSearchParams& params);

	std::queue<std::unique_ptr<ScraperRequest>> mRequestQueue;
	std::vector<ScraperSearchResult> mResults;
};

// This method uses the current scraper settings to pick the result source
std::unique_ptr<ScraperSearchHandle> startScraperSearch(const ScraperSearchParams& params);

namespace Scraper
{
	std::vector<std::string> getScraperList(); // returns valid scraper names
}

// Meta data asset downloading stuff.
class MDResolveHandle : public AsyncHandle
{
public:
	MDResolveHandle(const ScraperSearchResult& result, const ScraperSearchParams& search);

	const ScraperSearchResult& getResult() const
	{
		assert(mStatus == AsyncHandleStatus::Done);
		return mResult;
	}

private:
	void update() override;

	ScraperSearchResult mResult;

	typedef std::pair<std::unique_ptr<AsyncHandle>, std::function<void()>> ResolvePair;
	std::vector<ResolvePair> mFuncs;
};

class ImageDownloadHandle : public AsyncHandle
{
public:
	ImageDownloadHandle(const std::string& url, const std::string& path, int maxWidth, int maxHeight);

private:
	void update() override;

	std::unique_ptr<HttpReq> mReq;
	std::string mSavePath;
	int mMaxWidth;
	int mMaxHeight;
};

// About the same as "~/.emulationstation/downloaded_images/[system_name]/[game_name].[url's extension]".
// Will create the "downloaded_images" and "subdirectory" directories if they do not exist.
std::string getSaveAsPath(const ScraperSearchParams& params, const std::string& suffix, const std::string& url);

// Will resize according to Settings::getInt("ScraperResizeWidth") and Settings::getInt("ScraperResizeHeight").
std::unique_ptr<ImageDownloadHandle> downloadImageAsync(const std::string& url, const std::string& saveAs);

// Resolves all metadata assets that need to be downloaded.
std::unique_ptr<MDResolveHandle> resolveMetaDataAssets(const ScraperSearchResult& result, const ScraperSearchParams& search);

// You can pass 0 for maxWidth or maxHeight to automatically keep the aspect ratio.
// Will overwrite the image at [path] with the new resized one.
// Returns true if successful, false otherwise.
bool resizeImage(const std::string& path, int maxWidth, int maxHeight);

#endif