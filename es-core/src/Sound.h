#pragma once
#include <memory>
#include <string>

struct Mix_Chunk;
class ThemeData;

class Sound
{
	std::string mPath;
	Mix_Chunk* mSampleData;
	bool playing;

public:
	static std::shared_ptr<Sound> get(const std::string& path);
	static std::shared_ptr<Sound> getFromTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& elem);

	~Sound();

	void init();
	void deinit();

	void loadFile(const std::string& path);

	void play();
	void stop();

private:
	Sound(const std::string& path);
};
