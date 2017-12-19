#if defined(EXTENSION)
#pragma once
#include "GuiComponent.h"
#include "components/BusyComponent.h"
#include "components/MenuComponent.h"

namespace boost
{
	class thread;
}

class GuiUpdate : public GuiComponent
{
public:
	GuiUpdate(Window* window);
	virtual ~GuiUpdate();

	void render(const Eigen::Affine3f& parentTrans) override;
	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;

private:
	BusyComponent mBusyAnim;
	bool mLoading;
	enum class State
	{
		Null = -2,
		Done = -1,
		Waiting,
		Initial,
		Downloading,
		PingError,
		UpdateReady,
		UpdateFailed,
		NoUpdate,
	} mState;
	std::pair<std::string, int> mResult;

	boost::thread* mHandle;
	boost::thread* mPingHandle;

	void threadUpdate();
	void threadPing();
};
#endif