#pragma once
#include "scrapers/Scraper.h"

void mamedb_generate_scraper_requests(
	const ScraperSearchParams& params, std::queue<std::unique_ptr<ScraperRequest>>& requests, std::vector<ScraperSearchResult>& results);

void mamedb_process_httpreq(const std::unique_ptr<HttpReq>& req, std::vector<ScraperSearchResult>& results);

class MamedbRequest final : public ScraperHttpRequest
{
public:
	MamedbRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url)
		: ScraperHttpRequest(resultsWrite, url)
	{
	}

private:
	void process(const std::unique_ptr<HttpReq>& req, std::vector<ScraperSearchResult>& results) override;
};
