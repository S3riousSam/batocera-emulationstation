#if defined(EXTENSION)
#include "Music.h"
#include "AudioManager.h"
#include "Log.h"
#include "ThemeData.h"
#include <SDL_mixer.h>
#include <map>

namespace
{
	static std::map<std::string, std::shared_ptr<Music>> sMap;
}

std::shared_ptr<Music> Music::get(const std::string& path)
{
	auto it = sMap.find(path);
	if (it != sMap.end())
		return it->second;
	std::shared_ptr<Music> music = std::shared_ptr<Music>(new Music(path));
	sMap[path] = music;
	AudioManager::getInstance()->registerMusic(music);
	return music;
}

std::shared_ptr<Music> Music::getFromTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element)
{
	LOG(LogInfo) << " req music [" << view << "." << element << "]";
	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "sound");
	if (elem == nullptr || !elem->has("path"))
	{
		LOG(LogInfo) << "   (missing)";
		return nullptr;
	}
	return get(elem->get<std::string>("path"));
}

Music::Music(const std::string& path)
	: mPath(path)
	, music(nullptr)
	, playing(false)
{
	initMusic();
}

Music::~Music()
{
	deinitMusic();
}

void Music::initMusic()
{
	if (music != nullptr)
		deinitMusic();

	if (mPath.empty())
		return;

	Mix_Music* mix = Mix_LoadMUS(mPath.c_str()); // load waves file via SDL
	if (mix == nullptr)
	{
		LOG(LogError) << "Error loading sound \"" << mPath << "\"!\n " << SDL_GetError();
		return;
	}

	music = mix;
}

void Music::deinitMusic()
{
	playing = false;
	if (music != nullptr)
	{
		Mix_FreeMusic(music);
		music = nullptr;
	}
}

void Music::play(bool repeat, void (*callback)())
{
	if (music == nullptr)
		return;

	if (!playing)
		playing = true;

	LOG(LogInfo) << "playing";
	if (Mix_FadeInMusic(music, repeat ? -1 : 1, 1000) == -1)
	{
		LOG(LogInfo) << "Mix_PlayMusic: " << Mix_GetError();
		return;
	}

	if (!repeat)
		Mix_HookMusicFinished(callback);
}
#endif