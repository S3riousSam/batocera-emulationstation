#pragma once
#include <string>

enum class AsyncHandleStatus
{
	Progressing,
	Error,
	Done
};

// Abstract class handling asynchronous operations.
class AsyncHandle
{
public:
	virtual ~AsyncHandle() = default;

	// Update and return the latest status.
	inline AsyncHandleStatus status()
	{
		update();
		return mStatus;
	}

	// User-friendly string of our current status.  Will return error message if status() == SEARCH_ERROR.
	inline std::string getStatusString()
	{
		switch (mStatus)
		{
		case AsyncHandleStatus::Progressing:
			return "in progress";
		case AsyncHandleStatus::Error:
			return mError;
		case AsyncHandleStatus::Done:
			return "done";
		default:
			return "something impossible has occured; row, row, fight the power";
		}
	}

protected:
	AsyncHandle()
		: mStatus(AsyncHandleStatus::Progressing)
	{
	}

	inline void setStatus(AsyncHandleStatus status)
	{
		mStatus = status;
	}
	inline void setError(const std::string& error)
	{
		setStatus(AsyncHandleStatus::Error);
		mError = error;
	}

	std::string mError;
	AsyncHandleStatus mStatus;

private:
	virtual void update() = 0;
};
