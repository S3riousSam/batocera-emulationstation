#if defined(EXTENSION)
#include "MamedbScraper.h"
#include "Log.h"
#include <boost/regex.hpp>

void mamedb_generate_scraper_requests(
	const ScraperSearchParams& params, std::queue<std::unique_ptr<ScraperRequest>>& requests, std::vector<ScraperSearchResult>& results)
{
	const std::string cleanName = params.game->getPath().filename().replace_extension("").generic_string();
	const std::string path = "www.mamedb.com/game/" + HttpReq::urlEncode(cleanName);
	requests.push(std::unique_ptr<ScraperRequest>(new MamedbRequest(results, path)));
}

void MamedbRequest::process(const std::unique_ptr<HttpReq>& req, std::vector<ScraperSearchResult>& results)
{
	const char* const TITLE_REGEX = "^.*?<b>Name:&nbsp</b>(?<title>.*?)<br/>.*?<b>Year:&nbsp</b> "
		"<a href='/year/.*?'>(?<date>.*?)</a><br/>"
		"<b>Manufacturer:&nbsp</b> <a href='/manufacturer/.*?'>(?<developer>.*?)</a><br/>"
		"<b>Filename:&nbsp;</b>(?<filename>.*?)<br/><b>.*$";
	//const char* const CLONE_REGEX = "^.*?&nbsp;\\(clone of: <a href='.*?'>(?<clone>.*?)</a>\\)&nbsp;<br/>.*$";

	assert(req->status() == HttpReq::Status::Success);

	boost::smatch infolinematches;
	if (boost::regex_match(req->getContent(), infolinematches, boost::regex("^.*?<h1>Game Details</h1>(.*?)</table>.*$")))
	{
		boost::smatch linematches;
		const std::string line(infolinematches[1]);
		if (boost::regex_match(line, linematches, boost::regex(TITLE_REGEX)))
		{
			ScraperSearchResult result;

			// TITLE
			result.mdl.set("name", std::string(linematches["title"]));
			boost::smatch tmatches;
			if (boost::regex_match(std::string(linematches["title"]), tmatches, boost::regex("^(?<title>.*?)&nbsp.*$")))
				result.mdl.set("name", std::string(tmatches["title"]));

			// DATE
			result.mdl.set("releasedate", std::string(linematches["date"]));

			// DEVELOPPER
			if (std::string(linematches["developer"]).compare(std::string("<unknown></unknown>")) != 0)
				result.mdl.set("developer", std::string(linematches["developer"]));

			// GENRE
			boost::smatch genrematches;
			if (boost::regex_match(line, genrematches, boost::regex("^.*?<b>Category:&nbsp;</b><a href='.*?'>(?<genre>.*?)</a><br/>.*$")))
				result.mdl.set("genre", std::string(genrematches["genre"]));

			// RATING
			boost::smatch scorematches;
			if (boost::regex_match(req->getContent(), scorematches, boost::regex("^.*?<b>Score:&nbsp;</b>(?<rating>.*?) \\(.*? votes\\)<br/>.*$")))
			{
				float score = 0;
				std::stringstream(std::string(scorematches["rating"])) >> score;
				score = score / 10.0f;
				result.mdl.set("rating", std::to_string(score));
			}

			// PLAYERS
			boost::smatch playersmatches;
			if (boost::regex_match(line, playersmatches, boost::regex("^.*?<b>Players:&nbsp;</b>(?<players>.*?)<br/>.*$")))
				result.mdl.set("players", std::string(playersmatches["players"]));

			// IMAGES
			boost::smatch snapmatches;
			std::stringstream ss;
			if (boost::regex_match(req->getContent(), snapmatches, boost::regex("^.*?<img src='/snap/(?<img>.*?)\\.png'.*$")))
			{
				ss << "http://www.mamedb.com/snap/" << std::string(snapmatches["img"]) << ".png";
				result.imageUrl = ss.str();
				result.thumbnailUrl = ss.str();
			}

			results.push_back(result);
		}
	}
	else
	{
		LOG(LogInfo) << req->getContent().c_str() << TITLE_REGEX << "\nNot found";
	}
}
#endif