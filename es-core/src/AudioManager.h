#pragma once
#include "SDL_audio.h"
#include "Sound.h"
#include <memory>
#include <vector>
#include "Music.h"

class AudioManager
{
	static std::vector<std::shared_ptr<Sound>> sSoundVector;
	static std::vector<std::shared_ptr<Music>> sMusicVector;

	static std::shared_ptr<AudioManager> sInstance;
	std::shared_ptr<Music> currentMusic;

	AudioManager();

public:
	virtual ~AudioManager();

	static std::shared_ptr<AudioManager>& getInstance();

	void init();
	void deinit();

	void registerSound(std::shared_ptr<Sound>& sound);
	void unregisterSound(std::shared_ptr<Sound>& sound);

	void play();
	void stop();

	void stopMusic();
	void themeChanged(const std::shared_ptr<ThemeData>& theme);
	void resumeMusic();
	void playCheckSound();
	void registerMusic(std::shared_ptr<Music>& music);
	void unregisterMusic(std::shared_ptr<Music>& music);
	void musicEnd();



private:
	bool running;
	int lastTime = 0;

	std::shared_ptr<Music> getRandomMusic(std::string themeSoundDirectory);

	bool runningFromPlaylist;

	bool update(int curTime);

	std::string currentThemeMusicDirectory;

	void playRandomMusic();
};
