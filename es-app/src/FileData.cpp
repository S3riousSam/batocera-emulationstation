#include "FileData.h"
#if defined(EXTENSION)
#include "Log.h"
#endif
#include "SystemData.h"

namespace fs = boost::filesystem;

extern const char* mameNameToRealName[];

namespace
{
	const char* getCleanMameName(const char* from)
	{
		char const** mameNames = mameNameToRealName;

		while (*mameNames != NULL && strcmp(from, *mameNames) != 0)
			mameNames += 2;

		if (*mameNames)
			return *(mameNames + 1);

		return from;
	}
} // namespace

std::string removeParenthesis(const std::string& str)
{
	// remove anything in parenthesis or brackets
	// should be roughly equivalent to the regex replace "\((.*)\)|\[(.*)\]" with ""
	// I would love to just use regex, but it's not worth pulling in another boost lib for one function that is used once

	std::string ret = str;
	size_t start, end;

	static const int NUM_TO_REPLACE = 2;
	static const char toReplace[NUM_TO_REPLACE * 2] = {'(', ')', '[', ']'};

	bool done = false;
	while (!done)
	{
		done = true;
		for (int i = 0; i < NUM_TO_REPLACE; i++)
		{
			end = ret.find_first_of(toReplace[i * 2 + 1]);
			start = ret.find_last_of(toReplace[i * 2], end);

			if (start != std::string::npos && end != std::string::npos)
			{
				ret.erase(start, end - start + 1);
				done = false;
			}
		}
	}

	// also strip whitespace
	end = ret.find_last_not_of(' ');
	if (end != std::string::npos)
		end++;

	ret = ret.substr(0, end);

	return ret;
}

FileData::FileData(FileType type, const fs::path& path, SystemData* system)
	: mType(type)
	, mPath(path)
	, mSystem(system)
	, mParent(nullptr)
	, metadata(type == GAME ? GAME_METADATA : FOLDER_METADATA) // metadata is REALLY set in the constructor!
{
	// metadata needs at least a name field (since that's what getName() will return)
	if (metadata.get("name").empty())
		metadata.set("name", getCleanName());
#if defined(EXTENSION)
	metadata.set("system", system->getName());
#endif
}

FileData::~FileData()
{
	if (mParent != nullptr)
		mParent->removeChild(this);

#if !defined(EXTENSION)
	while (mChildren.size())
		delete mChildren.back();
#else
	clear();
#endif
}

std::string FileData::getCleanName() const
{
	std::string stem = mPath.stem().generic_string();
	if (mSystem && (mSystem->hasPlatformId(PlatformIds::ARCADE) || mSystem->hasPlatformId(PlatformIds::NEOGEO)))
		stem = getCleanMameName(stem.c_str());
#if defined(EXTENSION)
	return stem;
#endif

#if !defined(EXTENSION)
	return removeParenthesis(stem);
#endif
}

const std::string& FileData::getThumbnailPath() const
{
	return metadata.get(!metadata.get("thumbnail").empty() ? "thumbnail" : "image");
}

std::vector<FileData*> FileData::getFilesRecursive(unsigned int typeMask) const
{
	std::vector<FileData*> out;

	for (const auto& it : mChildren)
	{
		if (it->getType() & typeMask)
			out.push_back(it);

		if (it->getChildren().size() > 0)
		{
			const std::vector<FileData*> subchildren = it->getFilesRecursive(typeMask);
			out.insert(out.end(), subchildren.cbegin(), subchildren.cend());
		}
	}

	return out;
}

void FileData::addChild(FileData* file)
{
	assert(mType == FOLDER);
	assert(file->getParent() == NULL);

	mChildren.push_back(file);
	file->mParent = this;
}

void FileData::removeChild(FileData* file)
{
	assert(mType == FOLDER);
	assert(file->getParent() == this);

	for (auto it = mChildren.begin(); it != mChildren.end(); it++)
	{
		if (*it == file)
		{
			mChildren.erase(it);
			return;
		}
	}

	// File somehow wasn't in our children.
	assert(false);
}

void FileData::sort(ComparisonFunction& comparator, bool ascending)
{
	std::sort(mChildren.begin(), mChildren.end(), comparator);

	for (auto& child : mChildren)
	{
		if (child->getChildren().size() > 0)
			child->sort(comparator, ascending); // Recursive call
	}

	if (!ascending)
		std::reverse(mChildren.begin(), mChildren.end());
}

void FileData::sort(const SortType& type)
{
	sort(*type.comparisonFunction, type.ascending);
}

#if defined(EXTENSION)
std::vector<FileData*> getRecursive(const char* label, unsigned int typeMask, const std::vector<FileData*>& source)
{
	std::vector<FileData*> out;
	const std::vector<FileData*>& files = source;

	for (const auto& it : files)
	{
		if (it->metadata.get(label).compare("true") == 0)
			out.push_back(it);
	}

	return out;
}

std::vector<FileData*> FileData::getFavoritesRecursive(unsigned int typeMask) const
{
	return getRecursive("favorite", typeMask, getFilesRecursive(typeMask));
}

std::vector<FileData*> FileData::getHiddenRecursive(unsigned int typeMask) const
{
	return getRecursive("hidden", typeMask, getFilesRecursive(typeMask));
}

void FileData::changePath(const boost::filesystem::path& path)
{
	clear();

	mPath = path;

	// metadata needs at least a name field (since that's what getName() will return)
	if (metadata.get("name").empty())
		metadata.set("name", getCleanName());
}

void FileData::addAlreadyExisitingChild(FileData* file)
{
	assert(mType == FOLDER);
	mChildren.push_back(file);
}

void FileData::removeAlreadyExisitingChild(FileData* file)
{
	assert(mType == FOLDER);
	for (auto it = mChildren.begin(); it != mChildren.end(); it++)
	{
		if (*it == file)
		{
			mChildren.erase(it);
			return;
		}
	}
	assert(false); // File somehow wasn't in our children.
}

void FileData::clear()
{
	while (mChildren.size())
		delete mChildren.back();
}

void FileData::lazyPopulate(const std::vector<std::string>& searchExtensions, SystemData* systemData)
{
	clear();
	populateFolder(this, searchExtensions, systemData);
}

void FileData::populateFolder(FileData* folder, const std::vector<std::string>& searchExtensions, SystemData* systemData)
{
	const fs::path& folderPath = folder->getPath();
	if (!fs::is_directory(folderPath))
	{
		LOG(LogWarning) << "This path expression is not a folder: " << folderPath;
		return;
	}

	// Prevents symlink recursion
	if (fs::is_symlink(folderPath) && folderPath.generic_string().find(fs::canonical(folderPath).generic_string()) == 0)
	{
		LOG(LogWarning) << "Skipping infinitely recursive symlink: " << folderPath;
		return;
	}

	for (const auto& childDir : fs::directory_iterator(folderPath))
	{
		const fs::path& filePath = childDir.path();
		if (filePath.stem().empty())
			continue;

		// Folders may  also match a ROM extension and be added as games (i.e. for higan and DOSBox)
		// https://github.com/Aloshi/EmulationStation/issues/75

		bool isGame = false;
		if ((searchExtensions.empty() && !fs::is_directory(filePath)) ||
			// The configuration allows a list of extensions to be defined (delimited with a space)
			// So, we first get the extension of the file itself:
			(std::find(searchExtensions.begin(), searchExtensions.end(), filePath.extension().string()) != searchExtensions.end() &&
				filePath.filename().string().compare(0, 1, ".") != 0))
		{
			folder->addChild(new FileData(GAME, filePath.generic_string(), systemData));
			isGame = true;
		}

		// add directories that also do not match an extension as folders
		if (!isGame && fs::is_directory(filePath))
		{
			folder->addChild(new FileData(FOLDER, filePath.generic_string(), systemData));
		}
	}
}

void FileData::populateRecursiveFolder(FileData* folder, const std::vector<std::string>& searchExtensions, SystemData* systemData)
{
	const fs::path& folderPath = folder->getPath();
	if (!fs::is_directory(folderPath))
	{
		LOG(LogWarning) << "This path expression is not a folder: " << folderPath;
		return;
	}

	// Prevents symlink recursion
	if (fs::is_symlink(folderPath) && (folderPath.generic_string().find(fs::canonical(folderPath).generic_string()) == 0))
	{
		LOG(LogWarning) << "Skipping infinitely recursive symlink: " << folderPath;
		return;
	}

	for (const auto& childDir : fs::directory_iterator(folderPath))
	{
		const fs::path& filePath = childDir.path();
		if (filePath.stem().empty()) // Discards "." and ".." folders
			continue;

		// Folders may  also match a ROM extension and be added as games (i.e. for higan and DOSBox)
		// https://github.com/Aloshi/EmulationStation/issues/75

		bool isGame = false;
		if ((searchExtensions.empty() && !fs::is_directory(filePath)) ||
			// The configuration allows a list of extensions to be defined (delimited with a space)
			// So, we first get the extension of the file itself:
			(std::find(searchExtensions.begin(), searchExtensions.end(), filePath.extension().string()) != searchExtensions.end() &&
				filePath.filename().string().compare(0, 1, ".") != 0))
		{
			folder->addChild(new FileData(GAME, filePath.generic_string(), systemData));
			isGame = true;
		}

		// add directories that also do not match an extension as folders
		if (!isGame && fs::is_directory(filePath))
		{
			FileData* newFolder = new FileData(FOLDER, filePath.generic_string(), systemData);
			populateRecursiveFolder(newFolder, searchExtensions, systemData); // Recursive call

			// ignore folders that do not contain games
			if (newFolder->getChildren().size() == 0)
				delete newFolder;
			else
				folder->addChild(newFolder);
		}
	}
}
#endif
