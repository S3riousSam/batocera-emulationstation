#include "HttpReq.h"
#include "Log.h"
#include <boost/filesystem.hpp>
#include <iostream>

namespace
{
	CURLM* s_multi_handle = curl_multi_init();

	// god dammit libcurl why can't you have some way to check the status of an individual handle
	// why do I have to handle ALL messages at once
	std::map<CURL*, HttpReq*> s_requests;
}

std::string HttpReq::urlEncode(const std::string& s)
{
	const static std::string unreserved = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";

	std::string escaped;
	for (size_t i = 0; i < s.length(); i++)
	{
		if (unreserved.find_first_of(s[i]) != std::string::npos)
		{
			escaped.push_back(s[i]);
		}
		else
		{
			escaped.append("%");
			char buf[3];
			sprintf(buf, "%.2X", (unsigned char)s[i]);
			escaped.append(buf);
		}
	}
	return escaped;
}

#if defined(USEFUL)
bool HttpReq::isUrl(const std::string& str)
{
	return (!str.empty() && !boost::filesystem::exists(str) && (
		str.find("http://") != std::string::npos ||
		str.find("https://") != std::string::npos ||
		str.find("www.") != std::string::npos));
}
#endif

HttpReq::HttpReq(const std::string& url)
	: mHandle(curl_easy_init())
	, mStatus(Status::Processing)
{
	if (mHandle == nullptr)
	{
		setErrorState("curl_easy_init failed");
		return;
	}

	// set the url
	CURLcode err = curl_easy_setopt(mHandle, CURLOPT_URL, url.c_str());
	if (err != CURLE_OK)
	{
		setErrorState(curl_easy_strerror(err));
		return;
	}

	// tell curl how to write the data
	err = curl_easy_setopt(mHandle, CURLOPT_WRITEFUNCTION, &HttpReq::write_content);
	if (err != CURLE_OK)
	{
		setErrorState(curl_easy_strerror(err));
		return;
	}

	// give curl a pointer to this HttpReq so we know where to write the data *to* in our write function
	err = curl_easy_setopt(mHandle, CURLOPT_WRITEDATA, this);
	if (err != CURLE_OK)
	{
		setErrorState(curl_easy_strerror(err));
		return;
	}

	// add the handle to our multi
	CURLMcode merr = curl_multi_add_handle(s_multi_handle, mHandle);
	if (merr != CURLM_OK)
	{
		setErrorState(curl_multi_strerror(merr));
		return;
	}

	s_requests[mHandle] = this;
}

HttpReq::~HttpReq()
{
	if (mHandle != nullptr)
	{
		s_requests.erase(mHandle);

		const CURLMcode merr = curl_multi_remove_handle(s_multi_handle, mHandle);
		if (merr != CURLM_OK)
			LOG(LogError) << "Error removing curl_easy handle from curl_multi: " << curl_multi_strerror(merr);

		curl_easy_cleanup(mHandle);
	}
}

HttpReq::Status HttpReq::status()
{
	if (mStatus == Status::Processing)
	{
		int handle_count;
		const CURLMcode mcode = curl_multi_perform(s_multi_handle, &handle_count);
		if (mcode != CURLM_OK && mcode != CURLM_CALL_MULTI_PERFORM)
		{
			setErrorState(curl_multi_strerror(mcode));
			return mStatus;
		}

		int msgs_left;
		CURLMsg* msg;
		while (msg = curl_multi_info_read(s_multi_handle, &msgs_left))
		{
			if (msg->msg == CURLMSG_DONE)
			{
				HttpReq* req = s_requests[msg->easy_handle];

				if (req == nullptr)
				{
					LOG(LogError) << "Cannot find easy handle!";
					continue;
				}

				if (msg->data.result == CURLE_OK)
					req->mStatus = Status::Success;
				else
					req->setErrorState(curl_easy_strerror(msg->data.result));
			}
		}
	}

	return mStatus;
}

std::string HttpReq::getContent() const
{
	assert(mStatus == Status::Success);
	return mContent.str();
}

void HttpReq::setErrorState(const char* msg)
{
	mStatus = Status::Error;
	mErrorMsg = msg;
}

std::string HttpReq::getErrorMsg()
{
	return mErrorMsg;
}

// curl callback where:
// size = size of an element, nmemb = number of elements
// return value is number of elements successfully read
size_t HttpReq::write_content(void* buff, size_t size, size_t nmemb, void* req_ptr)
{
	std::stringstream& ss = static_cast<HttpReq*>(req_ptr)->mContent;
	ss.write(static_cast<char*>(buff), size * nmemb);
	return nmemb;
}
