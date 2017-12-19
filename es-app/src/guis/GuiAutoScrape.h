#if defined(EXTENSION)
#pragma once
#include "GuiComponent.h"
#include "components/BusyComponent.h"

namespace boost
{
	class thread;
}

class GuiAutoScrape : public GuiComponent
{
public:
	GuiAutoScrape(Window* window);
	virtual ~GuiAutoScrape();

	void render(const Eigen::Affine3f& parentTrans) override;
	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;

private:
	BusyComponent mBusyAnim;
	bool mLoading;

	enum class State
	{
		Done = -1,
		Waiting,
		Initial,
		Success,
		Error,
	} mState;
	std::pair<std::string, int> mResult;
	boost::thread* mHandle;

	void threadAutoScrape();
};
#endif