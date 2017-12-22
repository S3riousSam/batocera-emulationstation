#include "ResourceManager.h"
#include "../data/Resources.h"
#include "Log.h"
#include <boost/filesystem.hpp>
#include <fstream>

namespace fs = boost::filesystem;

auto array_deleter = [](unsigned char* p) { delete[] p; };
auto nop_deleter = [](unsigned char* p) {};

std::shared_ptr<ResourceManager> ResourceManager::sInstance = nullptr;

ResourceManager::ResourceManager()
{
}

std::shared_ptr<ResourceManager>& ResourceManager::getInstance()
{
	if (!sInstance)
		sInstance = std::shared_ptr<ResourceManager>(new ResourceManager());

	return sInstance;
}

ResourceData ResourceManager::getFileData(const std::string& path) const
{
	if (res2hMap.find(path) != res2hMap.end()) // embedded?
	{
		const Res2hEntry& embeddedEntry = res2hMap.find(path)->second;
		return ResourceData{ std::shared_ptr<unsigned char>(const_cast<unsigned char*>(embeddedEntry.data), nop_deleter), embeddedEntry.size };
	}

	return !fs::exists(path) // file doesn't exist?
		? ResourceData{nullptr, 0} // return an "empty" ResourceData
		: loadFile(path);
}

ResourceData ResourceManager::loadFile(const std::string& path) const
{
	std::ifstream stream(path, std::ios::binary);

	stream.seekg(0, stream.end);
	const size_t size = static_cast<size_t>(stream.tellg());
	stream.seekg(0, stream.beg);

	// supply custom deleter to properly free array
	std::shared_ptr<unsigned char> data(new unsigned char[size], array_deleter);
	stream.read((char*)data.get(), size);
	stream.close();

	return ResourceData{ data, size };
}

bool ResourceManager::fileExists(const std::string& path) const
{
	return (res2hMap.find(path) != res2hMap.end()) ? true : fs::exists(path); // Embedded file returns true
}

void ResourceManager::unloadAll()
{
	auto iter = mReloadables.begin();
	while (iter != mReloadables.end())
	{
		if (!iter->expired())
		{
			iter->lock()->unload(sInstance);
			iter++;
		}
		else
		{
			iter = mReloadables.erase(iter);
		}
	}
}

void ResourceManager::reloadAll()
{
	auto iter = mReloadables.begin();
	while (iter != mReloadables.end())
	{
		if (!iter->expired())
		{
			iter->lock()->reload(sInstance);
			iter++;
		}
		else
		{
			iter = mReloadables.erase(iter);
		}
	}
}

void ResourceManager::addReloadable(std::weak_ptr<IReloadable> reloadable)
{
	mReloadables.push_back(reloadable);
}
