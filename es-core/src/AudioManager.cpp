#include "AudioManager.h"
#include "Log.h"
#if defined(EXTENSION)
#include "Music.h"
#endif
#include "RecalboxConf.h"
#include "Settings.h"
#include "Sound.h"
#include "ThemeData.h"
#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_mixer.h>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <views/SystemView.h>
#if !defined(WIN32)
#include <unistd.h>
#endif
#include <time.h>
#include <boost/range/iterator_range.hpp>

#if defined(EXTENSION)
std::vector<std::shared_ptr<Music>> AudioManager::sMusicVector;
#endif
std::vector<std::shared_ptr<Sound>> AudioManager::sSoundVector;

std::shared_ptr<AudioManager> AudioManager::sInstance;

AudioManager::AudioManager()
#if defined(EXTENSION)
	: currentMusic(nullptr)
	, running(0)
#endif
{
	init();
}

AudioManager::~AudioManager()
{
	deinit();
}

std::shared_ptr<AudioManager>& AudioManager::getInstance()
{
	if (sInstance == nullptr)
		sInstance = std::shared_ptr<AudioManager>(new AudioManager);
	return sInstance;
}

void AudioManager::init()
{
#if defined(EXTENSION)
	runningFromPlaylist = false;
	if (running == 0)
	{
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
		{
			LOG(LogError) << "Error initializing SDL audio!\n" << SDL_GetError();
			return;
		}

		// Open the audio device and pause
		if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) < 0)
		{
			LOG(LogError) << "MUSIC Error - Unable to open SDLMixer audio: " << SDL_GetError() << std::endl;
		}
		else
		{
			LOG(LogInfo) << "SDL AUDIO Initialized";
			running = 1;
		}
	}
#endif
}

void AudioManager::deinit()
{
	// stop all playback
#if defined(EXTENSION)
	stop();
#endif
	// completely tear down SDL audio. else SDL hogs audio resources and emulators might fail to start...
	LOG(LogInfo) << "Shutting down SDL AUDIO";

	Mix_HaltMusic();
	Mix_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
#if defined(EXTENSION)
	running = 0;
#endif
}

#if defined(EXTENSION)
void AudioManager::stopMusic()
{
	Mix_FadeOutMusic(1000);
	Mix_HaltMusic();
	currentMusic = NULL;
}

void musicEndInternal()
{
	AudioManager::getInstance()->musicEnd();
}

void AudioManager::themeChanged(const std::shared_ptr<ThemeData>& theme)
{
	if (RecalboxConf::get("audio.bgmusic") == "1")
	{
		const ThemeData::ThemeElement* elem = theme->getElement("system", "directory", "sound");
		currentThemeMusicDirectory = (elem == nullptr || !elem->has("path")) ? std::string() : elem->get<std::string>("path");
		std::shared_ptr<Music> bgsound = Music::getFromTheme(theme, "system", "bgsound");

		// Found a music for the system
		if (bgsound)
		{
			runningFromPlaylist = false;
			stopMusic();
			bgsound->play(true, nullptr);
			currentMusic = bgsound;
			return;
		}

		if (!runningFromPlaylist)
			playRandomMusic();
	}
}

void AudioManager::playRandomMusic()
{ // Find a random song in user directory or theme music directory
	std::shared_ptr<Music> bgsound = getRandomMusic(currentThemeMusicDirectory);
	if (bgsound)
	{
		runningFromPlaylist = true;
		stopMusic();
		bgsound->play(false, musicEndInternal);
		currentMusic = bgsound;
	}
	else
	{
		stopMusic(); // Not running from play list, and no theme song found
	}
}

void AudioManager::resumeMusic()
{
	this->init();
	if (currentMusic != NULL && RecalboxConf::get("audio.bgmusic") == "1")
		currentMusic->play(runningFromPlaylist ? false : true, runningFromPlaylist ? musicEndInternal : NULL);
}
#endif

void AudioManager::registerSound(std::shared_ptr<Sound>& sound)
{
	//getInstance();
	sSoundVector.push_back(sound);
}
#if defined(EXTENSION)
void AudioManager::registerMusic(std::shared_ptr<Music>& music)
{
	//getInstance();
	sMusicVector.push_back(music);
}
#endif
void AudioManager::unregisterSound(std::shared_ptr<Sound>& sound)
{
	//getInstance();
	for (size_t i = 0; i < sSoundVector.size(); i++)
	{
		if (sSoundVector.at(i) == sound)
		{
			sSoundVector[i]->stop();
			sSoundVector.erase(sSoundVector.begin() + i);
			return;
		}
	}
	LOG(LogError) << "AudioManager Error - tried to unregister a sound that wasn't registered!";
}
#if defined(EXTENSION)
void AudioManager::unregisterMusic(std::shared_ptr<Music>& music)
{
	//getInstance();
	for (size_t i = 0; i < sMusicVector.size(); i++)
	{
		if (sMusicVector.at(i) == music)
		{
			//sMusicVector[i]->stop();
			sMusicVector.erase(sMusicVector.begin() + i);
			return;
		}
	}
	LOG(LogError) << "AudioManager Error - tried to unregister a music that wasn't registered!";
}
#endif

void AudioManager::play()
{
	//getInstance();
#if !defined(EXTENSION)
	SDL_PauseAudio(0); // Resuming audio, the mixer figures out if samples need to be played...
#endif
}

void AudioManager::stop()
{
	// stop playing all Sounds
	for (auto& it : sSoundVector)
		it->stop();
	// stop playing all Musics

#if !defined(EXTENSION)
	SDL_PauseAudio(1);
#endif
}

#if defined(EXTENSION)
namespace
{
	std::vector<std::string> getMusicIn(const std::string& path)
	{
		if (!boost::filesystem::is_directory(path))
			return std::vector<std::string>();

		std::vector<std::string> all_matching_files;
		for (const auto& i : boost::make_iterator_range(boost::filesystem::directory_iterator(path)))
		{
			if (boost::filesystem::is_regular_file(i.status()) &&
				boost::regex_match(i.path().string(), boost::smatch(), boost::regex(".*\\.(mp3|ogg)$")))
			{
				all_matching_files.push_back(i.path().string()); // File matches, store it
			}
		}
		return all_matching_files;
	}
}

std::shared_ptr<Music> AudioManager::getRandomMusic(const std::string& themeSoundDirectory)
{
	std::vector<std::string> musics = getMusicIn(Settings::getInstance()->getString("MusicDirectory"));
	if (musics.empty())
	{
		if (!themeSoundDirectory.empty())
		{
			musics = getMusicIn(themeSoundDirectory);
			if (musics.empty())
				return nullptr;
		}
		else
		{
			return nullptr;
		}
	}
#if defined(WIN32)
	srand(time(NULL) % getpid());
#else
	srand(time(NULL) % getpid() + getppid());
#endif
	return Music::get(musics.at(rand() % musics.size()));
}

void AudioManager::musicEnd()
{
	LOG(LogInfo) << "MusicEnded";
	if (runningFromPlaylist && RecalboxConf::get("audio.bgmusic") == "1")
		playRandomMusic();
}

void AudioManager::playCheckSound()
{
	const std::string selectedTheme = Settings::getInstance()->getString("ThemeSet");
	std::string loadingMusic = Platform::getHomePath() + "/.emulationstation/themes/" + selectedTheme + "/fx/loading.ogg";

	if (boost::filesystem::exists(loadingMusic) == false)
		loadingMusic = "/recalbox/share_init/system/.emulationstation/themes/" + selectedTheme + "/fx/loading.ogg";

	if (boost::filesystem::exists(loadingMusic))
		Music::get(loadingMusic)->play(false, NULL);
}

#endif