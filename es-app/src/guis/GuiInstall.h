#if defined(EXTENSION)
#pragma once
#include "GuiComponent.h"
#include "components/BusyComponent.h"
#include "components/MenuComponent.h"

namespace boost
{
	class thread;
}

class GuiInstall : public GuiComponent
{
public:
	GuiInstall(Window* window, const std::string& storageDevice, const std::string& architecture);
	virtual ~GuiInstall();

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

	const std::string mstorageDevice;
	const std::string marchitecture;

	boost::thread* mHandle;

	void threadInstall();
};
#endif