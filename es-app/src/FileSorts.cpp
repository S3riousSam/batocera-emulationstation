#include "FileSorts.h"
#include "LocaleES.h"

namespace FileSorts
{
	bool compareRating(const FileData* file1, const FileData* file2);
	bool compareTimesPlayed(const FileData* file1, const FileData* fil2);
	bool compareLastPlayed(const FileData* file1, const FileData* file2);

#if defined(EXTENSION)
	bool compareNumberPlayers(const FileData* file1, const FileData* file2);
	bool compareDevelopper(const FileData* file1, const FileData* file2);
	bool compareGenre(const FileData* file1, const FileData* file2);

	const std::vector<FileData::SortType> SortTypes =
	// warning C4566: character represented by universal-character-name '\uF15D' cannot be represented in the current code page (1252)
#if defined(WIN32) || defined(_WIN32)
#pragma warning(disable : 4566)
#endif
	{
		FileData::SortType(&compareFileName, true, std::string("\uF15d ") + _("FILENAME")),
		FileData::SortType(&compareFileName, false, std::string("\uF15e ") + _("FILENAME")),
		FileData::SortType(&compareRating, true, std::string("\uF165 ") + _("RATING")),
		FileData::SortType(&compareRating, false, std::string("\uF164 ") + _("RATING")),
		FileData::SortType(&compareTimesPlayed, true, std::string("\uF160 ") + _("TIMES PLAYED")),
		FileData::SortType(&compareTimesPlayed, false, std::string("\uF161 ") + _("TIMES PLAYED")),
		FileData::SortType(&compareLastPlayed, true, std::string("\uF160 ") + _("LAST PLAYED")),
		FileData::SortType(&compareLastPlayed, false, std::string("\uF161 ") + _("LAST PLAYED")),
		FileData::SortType(&compareNumberPlayers, true, std::string("\uF162 ") + _("NUMBER OF PLAYERS")),
		FileData::SortType(&compareNumberPlayers, false, std::string("\uF163 ") + _("NUMBER OF PLAYERS")),
		FileData::SortType(&compareDevelopper, true, std::string("\uF15d ") + _("DEVELOPER")),
		FileData::SortType(&compareDevelopper, false, std::string("\uF15e ") + _("DEVELOPER")),
		FileData::SortType(&compareGenre, true, std::string("\uF15d ") + _("GENRE")),
		FileData::SortType(&compareGenre, false, std::string("\uF15e ") + _("GENRE"))
	};
#if defined(WIN32) || defined(_WIN32)
#pragma warning(default : 4566)
#endif
#else
	const FileData::SortType typesArr[] = {FileData::SortType(&compareFileName, true, "filename, ascending"),
		FileData::SortType(&compareFileName, false, "filename, descending"),

		FileData::SortType(&compareRating, true, "rating, ascending"), FileData::SortType(&compareRating, false, "rating, descending"),

		FileData::SortType(&compareTimesPlayed, true, "times played, ascending"),
		FileData::SortType(&compareTimesPlayed, false, "times played, descending"),

		FileData::SortType(&compareLastPlayed, true, "last played, ascending"),
		FileData::SortType(&compareLastPlayed, false, "last played, descending")};

	const std::vector<FileData::SortType> SortTypes(typesArr, typesArr + sizeof(typesArr) / sizeof(typesArr[0]));
#endif

	bool compareTextMeta(const std::string& text1, const std::string& text2)
	{
		// min of text1/text2 .length()s
		const unsigned int count = text1.length() > text2.length() ? text2.length() : text1.length();
		for (unsigned int i = 0; i < count; i++)
		{
			if (toupper(text1[i]) != toupper(text2[i]))
				return toupper(text1[i]) < toupper(text2[i]);
		}

		return text1.length() < text2.length();
	}

	// returns if file1 should come before file2
	bool compareFileName(const FileData* file1, const FileData* file2)
	{
		return compareTextMeta(file1->getName(), file2->getName());
	}

	bool compareNumberMeta(const std::string& meta, const FileData* file1, const FileData* file2)
	{
		return // only games have [matea] metadata
			(file1->metadata.getType() == GAME_METADATA && file2->metadata.getType() == GAME_METADATA) &&
			(file1->metadata.getFloat(meta) < file2->metadata.getFloat(meta));
	}
	bool compareRating(const FileData* file1, const FileData* file2)
	{
		return compareNumberMeta("rating", file1, file2);
	}
	bool compareTimesPlayed(const FileData* file1, const FileData* file2)
	{
		return compareNumberMeta("playcount", file1, file2);
	}
	bool compareLastPlayed(const FileData* file1, const FileData* file2)
	{
		return compareNumberMeta("lastplayed", file1, file2);
	}

#if defined(EXTENSION)
	bool compareNumberPlayers(const FileData* file1, const FileData* file2)
	{
		return compareNumberMeta("players", file1, file2);
	}

	bool compareDevelopper(const FileData* file1, const FileData* file2)
	{
		if (file1->metadata.getType() == GAME_METADATA && file2->metadata.getType() == GAME_METADATA)
			return compareTextMeta(file1->metadata.get("developer"), file2->metadata.get("developer"));

		return false;
	}

	bool compareGenre(const FileData* file1, const FileData* file2)
	{
		if (file1->metadata.getType() == GAME_METADATA && file2->metadata.getType() == GAME_METADATA)
			return compareTextMeta(file1->metadata.get("genre"), file2->metadata.get("genre"));

		return false;
	}
#endif
}; // namespace FileSorts
