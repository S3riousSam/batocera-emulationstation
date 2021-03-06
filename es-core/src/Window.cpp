#include "Window.h"
#include "AudioManager.h"
#include "GuiComponent.h"
#include "InputManager.h"
#include "Log.h"
#include "RecalboxConf.h"
#include "Renderer.h"
#include "Settings.h"
#include "SystemInterface.h"
#include "components/HelpComponent.h"
#include "components/ImageComponent.h"
#include "guis/GuiMsgBox.h"
#include <iomanip>
#include <iostream>
#if defined(EXTENSION)
#include "LocaleES.h"
#else
#include <boost/locale.hpp>
#define _(A) A
#endif
#include <algorithm>

Window::Window()
	: mNormalizeNextUpdate(false)
	, mFrameTimeElapsed(0)
	, mFrameCountElapsed(0)
	, mAverageDeltaTime(10)
	, mAllowSleep(true)
	, mSleeping(false)
	, mTimeSinceLastInput(0)
	, mLaunchKodi(false)
{
	mHelp = new HelpComponent(this);
	mBackgroundOverlay = new ImageComponent(this);
	mBackgroundOverlay->setImage(":/scroll_gradient.png");
}

Window::~Window()
{
	delete mBackgroundOverlay;

	// delete GUI components
	while (peekGui() != nullptr)
		delete peekGui();

	delete mHelp;
}

void Window::pushGui(GuiComponent* gui)
{
	mGuiStack.push_back(gui);
	gui->updateHelpPrompts();
}

void Window::displayMessage(const std::string& message)
{
	mMessages.push_back(message);
}

void Window::removeGui(GuiComponent* gui)
{
	for (auto i = mGuiStack.begin(); i != mGuiStack.end(); i++)
	{
		if (*i == gui)
		{
			i = mGuiStack.erase(i);

			if (i == mGuiStack.end() && mGuiStack.size()) // we just popped the stack and the stack is not empty
				mGuiStack.back()->updateHelpPrompts();

			return;
		}
	}
}

GuiComponent* Window::peekGui()
{
	return (mGuiStack.size() == 0) ? nullptr : mGuiStack.back();
}

bool Window::init(unsigned int width, unsigned int height, bool initRenderer)
{
	if (initRenderer)
	{
		if (!Renderer::init(width, height))
		{
			LOG(LogError) << "Renderer failed to initialize!";
			return false;
		}
	}

	InputManager::getInstance()->init();

	ResourceManager::getInstance()->reloadAll();

	// keep a reference to the default fonts, so they don't keep getting destroyed/recreated
	if (mDefaultFonts.empty())
	{
		mDefaultFonts.push_back(Font::get(FONT_SIZE_SMALL));
		mDefaultFonts.push_back(Font::get(FONT_SIZE_MEDIUM));
		mDefaultFonts.push_back(Font::get(FONT_SIZE_LARGE));
	}

	mBackgroundOverlay->setResize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	// update our help because font sizes probably changed
	if (peekGui() != nullptr)
		peekGui()->updateHelpPrompts();

	return true;
}

void Window::deinit()
{
	InputManager::getInstance()->deinit();
	ResourceManager::getInstance()->unloadAll();
	Renderer::deinit();
}

void Window::textInput(const char* text)
{
	if (peekGui() != nullptr)
		peekGui()->textInput(text);
}

void Window::input(InputConfig* config, Input input)
{
	if (mSleeping)
	{
		// wake up
		mTimeSinceLastInput = 0;
		mSleeping = false;
		onWake();
		return;
	}

	mTimeSinceLastInput = 0;

	if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_g && SDL_GetModState() & KMOD_LCTRL && Settings::getInstance()->getBool("Debug"))
	{
		// toggle debug grid with Ctrl-G
		Settings::getInstance()->setBool("DebugGrid", !Settings::getInstance()->getBool("DebugGrid"));
	}
	else if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_t && SDL_GetModState() & KMOD_LCTRL && Settings::getInstance()->getBool("Debug"))
	{
		// toggle TextComponent debug view with Ctrl-T
		Settings::getInstance()->setBool("DebugText", !Settings::getInstance()->getBool("DebugText"));
	}
#if defined(EXTENSION)
	else if (config->getDeviceId() == DEVICE_KEYBOARD && input.value && input.id == SDLK_F1)
	{
		SystemInterface::launchFileManager(this);
	}
#endif
	else
	{
#if defined(EXTENSION)
		if (config->isMappedTo("x", input) && input.value && !mLaunchKodi && RecalboxConf::get("kodi.enabled") == "1" &&
			RecalboxConf::get("kodi.xbutton") == "1")
		{
			mLaunchKodi = true;
			this->pushGui(new GuiMsgBox(this, _("DO YOU WANT TO START KODI MEDIA CENTER?"), _("YES"),
				[this] {
					if (!SystemInterface::launchKodi(this))
						LOG(LogWarning) << "Shutdown terminated with non-zero result!";
					mLaunchKodi = false;
				},
				_("NO"), [this] { mLaunchKodi = false; }));
		}
		else
#endif
		{
			if (peekGui() != nullptr)
				this->peekGui()->input(config, input);
		}
	}
}

void Window::update(int deltaTime)
{
#if defined(EXTENSION)
	if (!mMessages.empty())
	{
		const std::string message = mMessages.back();
		mMessages.pop_back();
		pushGui(new GuiMsgBox(this, message));
	}
#endif
	if (mNormalizeNextUpdate)
	{
		mNormalizeNextUpdate = false;
		if (deltaTime > mAverageDeltaTime)
			deltaTime = mAverageDeltaTime;
	}

	mFrameTimeElapsed += deltaTime;
	mFrameCountElapsed++;
	if (mFrameTimeElapsed > 500)
	{
		mAverageDeltaTime = mFrameTimeElapsed / mFrameCountElapsed;

		if (Settings::getInstance()->getBool("DrawFramerate"))
		{
			std::stringstream ss;

			// fps
			ss << std::fixed << std::setprecision(1) << (1000.0f * (float)mFrameCountElapsed / (float)mFrameTimeElapsed) << "fps, ";
			ss << std::fixed << std::setprecision(2) << ((float)mFrameTimeElapsed / (float)mFrameCountElapsed) << "ms";

			// vram
			float textureVramUsageMb = TextureResource::getTotalMemUsage() / 1000.0f / 1000.0f;
			;
			float fontVramUsageMb = Font::getTotalMemUsage() / 1000.0f / 1000.0f;
			;
			float totalVramUsageMb = textureVramUsageMb + fontVramUsageMb;
			ss << "\nVRAM: " << totalVramUsageMb << "mb (texs: " << textureVramUsageMb << "mb, fonts: " << fontVramUsageMb << "mb)";

			mFrameDataText = std::unique_ptr<TextCache>(mDefaultFonts.at(1)->buildTextCache(ss.str(), 50.f, 50.f, 0xFF00FFFF));
		}

		mFrameTimeElapsed = 0;
		mFrameCountElapsed = 0;
	}

	mTimeSinceLastInput += deltaTime;

	if (peekGui() != nullptr)
		peekGui()->update(deltaTime);
}

void Window::render()
{
	const Eigen::Affine3f transform = Eigen::Affine3f::Identity();

	mRenderedHelpPrompts = false;

	// draw only bottom and top of GuiStack (if they are different)
	if (mGuiStack.size())
	{
		auto& bottom = mGuiStack.front();
		auto& top = mGuiStack.back();

		bottom->render(transform);
		if (bottom != top)
		{
			mBackgroundOverlay->render(transform);
			top->render(transform);
		}
	}

	if (!mRenderedHelpPrompts)
		mHelp->render(transform);

	if (Settings::getInstance()->getBool("DrawFramerate") && mFrameDataText)
	{
		Renderer::setMatrix(Eigen::Affine3f::Identity());
		mDefaultFonts.at(1)->renderTextCache(mFrameDataText.get());
	}

	const unsigned int screensaverTime = (unsigned int)Settings::getInstance()->getInt("ScreenSaverTime");
	if (mTimeSinceLastInput >= screensaverTime && screensaverTime != 0)
	{
#if defined(EXTENSION)
		{ // Render screen saver
			Renderer::setMatrix(Eigen::Affine3f::Identity());
			unsigned char opacity = Settings::getInstance()->getString("ScreenSaverBehavior") == "dim" ? 0xA0 : 0xFF;
			Renderer::drawRect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(), 0x00000000 | opacity);
		}

		if (!isProcessing() && mAllowSleep)
#endif
		{
			// go to sleep
			mSleeping = true;
			onSleep();
		}
	}
}

void Window::normalizeNextUpdate()
{
	mNormalizeNextUpdate = true;
}

bool Window::getAllowSleep()
{
	return mAllowSleep;
}

void Window::setAllowSleep(bool sleep)
{
	mAllowSleep = sleep;
}

void Window::renderWaitingScreen(const std::string& text)
{
	Eigen::Affine3f trans = Eigen::Affine3f::Identity();
	Renderer::setMatrix(trans);
	Renderer::drawRect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(), 0xFFFFFFFF);

	ImageComponent splash(this);
	splash.setResize(Renderer::getScreenWidth() * 0.6f, 0.0f);
	splash.setImage(":/splash.svg");
	splash.setPosition((Renderer::getScreenWidth() - splash.getSize().x()) / 2, (Renderer::getScreenHeight() - splash.getSize().y()) / 2 * 0.6f);
	splash.render(trans);

	auto& font = mDefaultFonts.at(1);
	TextCache* cache = font->buildTextCache(text, 0, 0, COLOR_GRAY4);
	trans = trans.translate(
		Eigen::Vector3f(round((Renderer::getScreenWidth() - cache->metrics.size.x()) / 2.0f), round(Renderer::getScreenHeight() * 0.835f), 0.0f));
	Renderer::setMatrix(trans);
	font->renderTextCache(cache);
	delete cache;

	Renderer::swapBuffers();
}

void Window::renderLoadingScreen()
{
	renderWaitingScreen(_("LOADING..."));
}
void Window::renderShutdownScreen()
{
	renderWaitingScreen(_("PLEASE WAIT..."));
}

void Window::renderHelpPromptsEarly()
{
	mHelp->render(Eigen::Affine3f::Identity());
	mRenderedHelpPrompts = true;
}

void Window::setHelpPrompts(const std::vector<HelpPrompt>& prompts, const HelpStyle& style)
{
	mHelp->clearPrompts();
	mHelp->setStyle(style);

	std::vector<HelpPrompt> addPrompts;

	std::map<std::string, bool> inputSeenMap;
	std::map<std::string, int> mappedToSeenMap;
	for (auto it = prompts.begin(); it != prompts.end(); it++)
	{
		// only add it if the same icon hasn't already been added
		if (inputSeenMap.insert(std::make_pair<std::string, bool>(it->first.c_str(), true)).second)
		{
			// this symbol hasn't been seen yet, what about the action name?
			auto mappedTo = mappedToSeenMap.find(it->second);
			if (mappedTo != mappedToSeenMap.end())
			{
				// yes, it has!

				// can we combine? (dpad only)
				if ((strcmp(it->first.c_str(), "up/down") == 0 && strcmp(addPrompts.at(mappedTo->second).first.c_str(), "left/right") == 0) ||
					(strcmp(it->first.c_str(), "left/right") == 0 && strcmp(addPrompts.at(mappedTo->second).first.c_str(), "up/down") == 0))
				{
					// yes!
					addPrompts.at(mappedTo->second).first = "up/down/left/right";
					// don't need to add this to addPrompts since we just merged
				}
				else
				{
					// no, we can't combine!
					addPrompts.push_back(*it);
				}
			}
			else
			{
				// no, it hasn't!
				mappedToSeenMap.insert(std::pair<std::string, int>(it->second, addPrompts.size()));
				addPrompts.push_back(*it);
			}
		}
	}

	// sort prompts so it goes [dpad_all] [dpad_u/d] [dpad_l/r] [a/b/x/y/l/r] [start/select]
	std::sort(addPrompts.begin(), addPrompts.end(), [](const HelpPrompt& a, const HelpPrompt& b) -> bool {

		static const char* map[] = {"up/down/left/right", "up/down", "left/right", "a", "b", "x", "y", "l", "r", "start", "select", NULL};

		int i = 0;
		int aVal = 0;
		int bVal = 0;
		while (map[i] != NULL)
		{
			if (a.first == map[i])
				aVal = i;
			if (b.first == map[i])
				bVal = i;
			i++;
		}

		return aVal > bVal;
	});

	mHelp->setPrompts(addPrompts);
}

void Window::onSleep()
{
#if !defined(EXTENSION)
	Renderer::setMatrix(Eigen::Affine3f::Identity());
	unsigned char opacity = Settings::getInstance()->getString("ScreenSaverBehavior") == "dim" ? 0xA0 : 0xFF;
	Renderer::drawRect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(), 0x00000000 | opacity);
#endif
}

void Window::onWake()
{
}

bool Window::isProcessing()
{
	return count_if(mGuiStack.begin(), mGuiStack.end(), [](GuiComponent* c) { return c->isProcessing(); }) > 0;
}
