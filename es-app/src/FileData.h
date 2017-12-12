#pragma once

#include "MetaData.h"
#include <boost/filesystem.hpp>
#include <string>
#include <vector>

class SystemData;

enum FileType
{
	GAME = 1, // Cannot have children.
	FOLDER = 2
};

enum FileChangeType
{
	FILE_ADDED,
	FILE_METADATA_CHANGED,
	FILE_REMOVED,
	FILE_SORTED
};

// Used for loading/saving gamelist.xml.
const char* fileTypeToString(FileType type);
FileType stringToFileType(const char* str);

// Remove (.*) and [.*] from str
std::string removeParenthesis(const std::string& str);

// A tree node that holds information for a file.
class FileData
{
public:
	FileData(FileType type, const boost::filesystem::path& path, SystemData* system);
	virtual ~FileData();

	inline const std::string& getName() const
	{
		return metadata.get("name");
	}
	inline FileType getType() const
	{
		return mType;
	}
	inline const boost::filesystem::path& getPath() const
	{
		return mPath;
	}
	inline FileData* getParent() const
	{
		return mParent;
	}
	inline const std::vector<FileData*>& getChildren() const
	{
		return mChildren;
	}
	inline SystemData* getSystem() const
	{
		return mSystem;
	}

	virtual const std::string& getThumbnailPath() const;

	std::vector<FileData*> getFilesRecursive(unsigned int typeMask) const;

	void addChild(FileData* file); // Error if mType != FOLDER
	void removeChild(FileData* file); // Error if mType != FOLDER

	// Returns our best guess at the "real" name for this file (will strip parenthesis and attempt to perform MAME name translation)
	std::string getCleanName() const;

	typedef bool ComparisonFunction(const FileData* a, const FileData* b);
	struct SortType
	{
		ComparisonFunction* comparisonFunction;
		const bool ascending;
		const std::string description;

		SortType(ComparisonFunction* sortFunction, bool sortAscending, const std::string& sortDescription)
			: comparisonFunction(sortFunction)
			, ascending(sortAscending)
			, description(sortDescription)
		{
		}
	};

	void sort(ComparisonFunction& comparator, bool ascending = true);
	void sort(const SortType& type);

#if defined(EXTENSION)
	std::vector<FileData*> getFavoritesRecursive(unsigned int typeMask) const;
	std::vector<FileData*> getHiddenRecursive(unsigned int typeMask) const;

	void changePath(const boost::filesystem::path& path);

	void addAlreadyExisitingChild(FileData* file);
	void removeAlreadyExisitingChild(FileData* file);

	void clear();
	void lazyPopulate(const std::vector<std::string>& searchExtensions = std::vector<std::string>(), SystemData* systemData = nullptr);

	static void populateFolder(
		FileData* folder, const std::vector<std::string>& searchExtensions, SystemData* systemData);
	static void populateRecursiveFolder(
		FileData* folder, const std::vector<std::string>& searchExtensions, SystemData* systemData);
#endif
	MetaDataList metadata;

private:
	FileType mType;
	boost::filesystem::path mPath;
	SystemData* mSystem;
	FileData* mParent;
	std::vector<FileData*> mChildren;
};
