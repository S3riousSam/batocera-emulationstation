#pragma once
#include <curl/curl.h>
#include <map>
#include <sstream>

// Usage:
// HttpReq myRequest("www.google.com/index.html");
// - for blocking behavior: while(myRequest.status() == HttpReq::REQ_IN_PROGRESS);
// - for non-blocking behavior: check if(myRequest.status() != HttpReq::REQ_IN_PROGRESS) in some sort of update method
// //once one of those completes, the request is ready
// if(myRequest.status() != REQ_SUCCESS)
//     LOG(LogError) << "HTTP request error - " << myRequest.getErrorMessage();
// else
//     std::string content = myRequest.getContent();
class HttpReq
{
public:
	HttpReq(const std::string& url);
	~HttpReq();

	enum class Status
	{
		Processing,
		Success,
		Error,
	};

	Status status(); // process any received data and return the status afterwards

	std::string getErrorMsg();

	std::string getContent() const; // mStatus must be REQ_SUCCESS

	static std::string urlEncode(const std::string& s);
#if defined(USEFUL)
	static bool isUrl(const std::string& s);
#endif

private:
	static size_t write_content(void* buff, size_t size, size_t nmemb, void* req_ptr);

	void setErrorState(const char* msg);

	CURL* mHandle;

	Status mStatus;

	std::stringstream mContent;
	std::string mErrorMsg;
};
