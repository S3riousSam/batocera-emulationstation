#pragma once
#include <memory>
#include <vector>

class Music; // EXTENSION
class Sound;
class ThemeData;

class AudioManager
{
	static std::shared_ptr<AudioManager> sInstance;
	static std::vector<std::shared_ptr<Sound>> sSoundVector;
#if defined(EXTENSION)
	static std::vector<std::shared_ptr<Music>> sMusicVector;
	std::shared_ptr<Music> currentMusic;
#endif

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

#if defined(EXTENSION)
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
#endif
};
