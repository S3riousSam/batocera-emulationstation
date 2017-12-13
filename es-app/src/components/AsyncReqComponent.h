#pragma once
#include "GuiComponent.h"
#include <functional>
#include <memory>

class HttpReq;

// Displays a simple UI animation showing the application has not frozen while performing HTTP request.
// The player may cancel the operation by pressing B.
//
// Usage example:
//  std::shared_ptr<HttpReq> httpreq = std::make_shared<HttpReq>("cdn.garcya.us", "/wp-content/uploads/2010/04/TD250.jpg");
//  AsyncReqComponent* req = new AsyncReqComponent(mWindow, httpreq,
//      [] (std::shared_ptr<HttpReq> r) { LOG(LogInfo) << "Request completed. Error, if any: " << r->getErrorMsg(); },
//      [] () { LOG(LogInfo) << "Request canceled"; });
//  mWindow->pushGui(req); // req deletes itself...
class AsyncReqComponent : public GuiComponent
{
public:
	AsyncReqComponent(Window* window, std::shared_ptr<HttpReq> req,
		std::function<void(std::shared_ptr<HttpReq>)> onSuccess,
		std::function<void()> onCancel = nullptr);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Eigen::Affine3f& parentTrans) override;

	std::vector<HelpPrompt> getHelpPrompts() override;

private:
	std::function<void(std::shared_ptr<HttpReq>)> mSuccessFunc;
	std::function<void()> mCancelFunc;

	unsigned int mTime;
	std::shared_ptr<HttpReq> mRequest;
};
