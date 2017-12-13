#include "components/AsyncReqComponent.h"
#include "HttpReq.h"
#include "Renderer.h"

#define BUTTON_BACK "a"
#define BUTTON_LAUNCH "b"

AsyncReqComponent::AsyncReqComponent(Window* window, std::shared_ptr<HttpReq> req,
	std::function<void(std::shared_ptr<HttpReq>)> onSuccess,
	std::function<void()> onCancel)
	: GuiComponent(window)
	, mSuccessFunc(onSuccess)
	, mCancelFunc(onCancel)
	, mTime(0)
	, mRequest(req)
{
}

bool AsyncReqComponent::input(InputConfig* config, Input input)
{
	if (input.value != 0 && config->isMappedTo(BUTTON_BACK, input))
	{
		if (mCancelFunc)
			mCancelFunc();

		delete this;
	}

	return true;
}

void AsyncReqComponent::update(int deltaTime)
{
	if (mRequest->status() != HttpReq::Status::Processing)
	{
		mSuccessFunc(mRequest);
		delete this;
		return;
	}

	mTime += deltaTime;
}

void AsyncReqComponent::render(const Eigen::Affine3f& parentTrans)
{
	Eigen::Affine3f trans = Eigen::Affine3f::Identity();
	trans = trans.translate(Eigen::Vector3f(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0));
	Renderer::setMatrix(trans);

	const Eigen::Vector3f point(cos(mTime * 0.01f) * 12, sin(mTime * 0.01f) * 12, 0);
	Renderer::drawRect(static_cast<int>(point.x()), static_cast<int>(point.y()), 8, 8, 0x0000FFFF);
}

std::vector<HelpPrompt> AsyncReqComponent::getHelpPrompts()
{
	return { HelpPrompt(BUTTON_BACK, "cancel") };
}
