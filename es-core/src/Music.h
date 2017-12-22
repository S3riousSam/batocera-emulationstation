#if defined(EXTENSION)
#pragma once
#include <memory>
#include <string>

typedef struct _Mix_Music Mix_Music;
class ThemeData;

class Music
{
	std::string mPath;
	Mix_Music* music;
	bool playing;

public:
	static std::shared_ptr<Music> getFromTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element);
	static std::shared_ptr<Music> get(const std::string& path);

	void play(bool repeat, void (*callback)());

	~Music();

private:
	Music(const std::string& path);

	void initMusic();
	void deinitMusic();
};
#endif
