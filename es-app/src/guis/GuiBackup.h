#if defined(EXTENSION)
#pragma once
#include "GuiComponent.h"
#include "components/BusyComponent.h"
#include "components/MenuComponent.h"

namespace boost
{
	class thread;
}

class GuiBackup : public GuiComponent
{
public:
	GuiBackup(Window* window, const std::string& storageDevice);
	virtual ~GuiBackup();

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

	boost::thread* mHandle;

	void threadBackup();
};
#endif