#pragma once
#include "FileData.h"
#include "MetaData.h"
#include "PlatformId.h"
#include "ThemeData.h"
#include "Window.h"
#include <string>
#include <vector>

class SystemData
{
private:
	friend SystemData* createSystem(pugi::xml_node system);
	SystemData(const std::string& name, const std::string& fullName, const std::string& startPath, const std::vector<std::string>& extensions,
		const std::string& command, const std::vector<PlatformIds::PlatformId>& platformIds, const std::string& themeFolder
#if defined(EXTENSION)
		,
		std::map<std::string, std::vector<std::string>*>* emulators
#endif
	);

#if defined(EXTENSION)
	SystemData(const std::string& name, const std::string& fullName, const std::string& command, const std::string& themeFolder,
		std::vector<SystemData*>* systems);
#endif
public:
	~SystemData();

	inline FileData* getRootFolder() const
	{
		return mRootFolder;
	};
	inline const std::string& getName() const
	{
		return mName;
	}
	inline const std::string& getFullName() const
	{
		return mFullName;
	}
	inline const std::string& getStartPath() const
	{
		return mStartPath;
	}
	inline const std::vector<std::string>& getExtensions() const
	{
		return mSearchExtensions;
	}
	inline const std::string& getThemeFolder() const
	{
		return mThemeFolder;
	}
#if defined(EXTENSION)
	inline bool getHasFavorites() const
	{
		return mHasFavorites;
	}
	inline bool isFavorite() const
	{
		return mIsFavorite;
	}
	inline std::vector<FileData*> getFavorites() const
	{
		return mRootFolder->getFavoritesRecursive(GAME);
	}
#endif

	inline const std::vector<PlatformIds::PlatformId>& getPlatformIds() const
	{
		return mPlatformIds;
	}
	inline bool hasPlatformId(PlatformIds::PlatformId id)
	{
		return std::find(mPlatformIds.begin(), mPlatformIds.end(), id) != mPlatformIds.end();
	}

	inline const std::shared_ptr<ThemeData>& getTheme() const
	{
		return mTheme;
	}

	std::string getGamelistPath(bool forWrite) const;
	bool hasGamelist() const;

	std::string getThemePath() const;

	unsigned int getGameCount() const;
#if defined(EXTENSION)
	unsigned int getFavoritesCount() const;
	unsigned int getHiddenCount() const;
#endif

	void launchGame(Window* window, FileData* game);

	static void deleteSystems();

	// Loads the system config file at getConfigPath() returning true if no errors were encountered.
	// An example will be written if the file doesn't exist.
	static bool loadConfig();

	static std::string getConfigPath(
		bool forWrite); // if forWrite, will only return ~/.emulationstation/es_systems.cfg, never /etc/emulationstation/es_systems.cfg

	static std::vector<SystemData*> sSystemVector;
#if defined(EXTENSION)
	static SystemData* getFavoriteSystem();
#endif

	inline std::vector<SystemData*>::const_iterator getIterator() const
	{
		return std::find(sSystemVector.begin(), sSystemVector.end(), this);
	};
	inline std::vector<SystemData*>::const_reverse_iterator getRevIterator() const
	{
		return std::find(sSystemVector.rbegin(), sSystemVector.rend(), this);
	};

	inline SystemData* getNext() const
	{
		auto it = getIterator();
		it++;
		if (it == sSystemVector.end())
			it = sSystemVector.begin();
		return *it;
	}

	inline SystemData* getPrev() const
	{
		auto it = getRevIterator();
		it++;
		if (it == sSystemVector.rend())
			it = sSystemVector.rbegin();
		return *it;
	}

	void loadTheme(); // Load or re-load theme.
#if defined(EXTENSION)
	void refreshRootFolder(); // refresh the ROMs files
	const std::map<std::string, std::vector<std::string>*>* getEmulators() const;
#endif

private:
	const std::string mName;
	const std::string mFullName;
	const std::string mStartPath;
	const std::vector<std::string> mSearchExtensions;
	const std::string mLaunchCommand;
	std::vector<PlatformIds::PlatformId> mPlatformIds;
	const std::string mThemeFolder;
	std::shared_ptr<ThemeData> mTheme;
#if defined(EXTENSION)
	bool mHasFavorites;
	bool mIsFavorite;
#endif
	void populateFolder(FileData* folder);

	FileData* mRootFolder;
#if defined(EXTENSION)
	const std::map<std::string, std::vector<std::string>*>* mEmulators;
#endif
};
