#pragma once
#include "FileData.h"
#include <vector>

namespace FileSorts
{
	bool compareFileName(const FileData* file1, const FileData* file2);

	extern const std::vector<FileData::SortType> SortTypes;
};
