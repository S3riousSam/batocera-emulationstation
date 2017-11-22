// Created by matthieu on 03/08/15.
#pragma once
#include "GuiComponent.h"
#include "components/BusyComponent.h"
#include "components/MenuComponent.h"

#include <boost/thread.hpp>

class GuiLoading : public GuiComponent
{
public:
	GuiLoading(Window* window, const std::function<void*()>& mFunc);
	GuiLoading(Window* window, const std::function<void*()>& mFunc, const std::function<void(void*)>& mFunc2);
	virtual ~GuiLoading();

	void render(const Eigen::Affine3f& parentTrans) override;
	bool input(InputConfig* config, Input input) override;
	std::vector<HelpPrompt> getHelpPrompts() override;
	void update(int deltaTime) override;

private:
	BusyComponent mBusyAnim;
	boost::thread* mHandle;
	bool mRunning;
	const std::function<void*()>& mFunc1;
	const std::function<void(void*)>& mFunc2;
	void threadLoading();
	void* mResult;
};