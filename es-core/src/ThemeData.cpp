#include "ThemeData.h"
#include "Log.h"
#include "Renderer.h"
#include "Settings.h"
#include "Sound.h"
#include "pugixml/pugixml.hpp"
#include "resources/Font.h"
#include "resources/TextureResource.h"
#include <boost/assign.hpp>

#include "components/ImageComponent.h"
#include "components/TextComponent.h"

namespace
{
	enum ElementPropertyType
	{
		NORMALIZED_PAIR,
		EP_PATH,
		EP_STRING,
		EP_COLOR,
		EP_FLOAT,
		EP_BOOLEAN
	};

	// This is a work around for some ambiguity that is introduced in C++11 that boost::assign::map_list_of leaves open.
	// We use makeMap(actualmap) to implicitly convert the boost::assign::map_list_of's return type to ElementMapType.
	// Problem exists with gcc 4.7 and Boost 1.51.  Works fine with MSVC2010 without this hack.
	typedef std::map<std::string, ElementPropertyType> ElementMapType;

	template<typename T>
	ElementMapType makeMap(const T& mapInit)
	{
		ElementMapType m = mapInit;
		return m;
	}

	// clang-format off
	const std::map<std::string, ElementMapType> sElementMap = boost::assign::map_list_of
		("image", makeMap(boost::assign::map_list_of
			("pos", NORMALIZED_PAIR)
			("size", NORMALIZED_PAIR)
			("maxSize", NORMALIZED_PAIR)
			("origin", NORMALIZED_PAIR)
			("path", EP_PATH)
			("tile", EP_BOOLEAN)
			("color", EP_COLOR)))
		("text", makeMap(boost::assign::map_list_of
			("pos", NORMALIZED_PAIR)
			("size", NORMALIZED_PAIR)
			("text", EP_STRING)
			("color", EP_COLOR)
			("fontPath", EP_PATH)
			("fontSize", EP_FLOAT)
			("alignment", EP_STRING)
			("forceUppercase", EP_BOOLEAN)
			("lineSpacing", EP_FLOAT)))
		("textlist", makeMap(boost::assign::map_list_of
			("pos", NORMALIZED_PAIR)
			("size", NORMALIZED_PAIR)
			("selectorColor", EP_COLOR)
			("selectedColor", EP_COLOR)
			("primaryColor", EP_COLOR)
			("secondaryColor", EP_COLOR)
			("fontPath", EP_PATH)
			("fontSize", EP_FLOAT)
			("scrollSound", EP_PATH)
			("alignment", EP_STRING)
			("horizontalMargin", EP_FLOAT)
			("forceUppercase", EP_BOOLEAN)
			("lineSpacing", EP_FLOAT)))
		("container", makeMap(boost::assign::map_list_of
			("pos", NORMALIZED_PAIR)
			("size", NORMALIZED_PAIR)))
		("ninepatch", makeMap(boost::assign::map_list_of
			("pos", NORMALIZED_PAIR)
			("size", NORMALIZED_PAIR)
			("path", EP_PATH)))
		("datetime", makeMap(boost::assign::map_list_of
			("pos", NORMALIZED_PAIR)
			("size", NORMALIZED_PAIR)
			("color", EP_COLOR)
			("fontPath", EP_PATH)
			("fontSize", EP_FLOAT)
			("forceUppercase", EP_BOOLEAN)))
		("rating", makeMap(boost::assign::map_list_of
			("pos", NORMALIZED_PAIR)
			("size", NORMALIZED_PAIR)
			("filledPath", EP_PATH)
			("unfilledPath", EP_PATH)))
		("sound", makeMap(boost::assign::map_list_of
			("path", EP_PATH)))
		("helpsystem", makeMap(boost::assign::map_list_of
			("pos", NORMALIZED_PAIR)
			("textColor", EP_COLOR)
			("iconColor", EP_COLOR)
			("fontPath", EP_PATH)
			("fontSize", EP_FLOAT))
		);
	// clang-format on
}

namespace fs = boost::filesystem;

#define MINIMUM_THEME_FORMAT_VERSION 3
#if defined(EXTENSION)
#define CURRENT_THEME_FORMAT_VERSION 4
#else
#define CURRENT_THEME_FORMAT_VERSION 3
#endif

// helper
std::string resolvePath(const char* in, const fs::path& relative)
{
	if (!in || in[0] == '\0')
		return in;

	fs::path relPath = relative.parent_path();

	boost::filesystem::path path(in);

	// we use boost filesystem here instead of just string checks because
	// some directories could theoretically start with ~ or .
	if (*path.begin() == "~")
	{
		path = Platform::getHomePath() + (in + 1);
	}
	else if (*path.begin() == ".")
	{
		path = relPath / (in + 1);
	}

	return path.generic_string();
}

ThemeData::ThemeData()
	: mVersion(0)
{
}

void ThemeData::loadFile(const std::string& path)
{
	mPaths.push_back(path);

	if (!fs::exists(path))
		throw ThemeException(mPaths) << "File does not exist!";

	mVersion = 0;
	mViews.clear();

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());
	if (!res)
		throw ThemeException(mPaths) << "XML parsing error: \n    " << res.description();

	pugi::xml_node root = doc.child("theme");
	if (!root)
		throw ThemeException(mPaths) << "Missing <theme> tag!";

	// parse version
	mVersion = root.child("formatVersion").text().as_float(-404);
	if (mVersion == -404)
	{
		throw ThemeException(mPaths) << "<formatVersion> tag missing!\n"
										"It's either out of date or you need to add <formatVersion>"
									 << CURRENT_THEME_FORMAT_VERSION << "</formatVersion> inside your <theme> tag.";
	}

	if (mVersion < MINIMUM_THEME_FORMAT_VERSION)
		throw ThemeException(mPaths) << "Theme uses format version " << mVersion << ". Minimum supported version is " << MINIMUM_THEME_FORMAT_VERSION << ".";

	parseIncludes(root);
	parseViews(root);
}

void ThemeData::parseIncludes(const pugi::xml_node& root)
{
	ThemeException error(mPaths);

	for (const pugi::xml_node& node : root.children("include"))
	{
		const char* relPath = node.text().get();
		const std::string path = resolvePath(relPath, mPaths.back());
		if (!ResourceManager::getInstance()->fileExists(path))
			throw error << "Included file \"" << relPath << "\" not found! (resolved to \"" << path << "\")";

		error << "    from included file \"" << relPath << "\":\n    ";

		mPaths.push_back(path);

		pugi::xml_document includeDoc;
		const pugi::xml_parse_result result = includeDoc.load_file(path.c_str());
		if (!result)
			throw error << "Error parsing file: \n    " << result.description();

		// WARNING: root hides function parameter!
		const pugi::xml_node root = includeDoc.child("theme");
		if (!root)
			throw error << "Missing <theme> tag!";

		parseIncludes(root); // Recursive call
		parseViews(root);

		mPaths.pop_back();
	}
}

void ThemeData::parseViews(const pugi::xml_node& root)
{
	// parse views
	for (const pugi::xml_node& node : root.children("view"))
	{
		if (!node.attribute("name"))
			throw ThemeException(mPaths) << "View missing \"name\" attribute!";

		const char* delim = " \t\r\n,";
		const std::string nameAttr = node.attribute("name").as_string();
		size_t prevOff = nameAttr.find_first_not_of(delim, 0);
		size_t off = nameAttr.find_first_of(delim, prevOff);
		std::string viewKey;
		while (off != std::string::npos || prevOff != std::string::npos)
		{
			viewKey = nameAttr.substr(prevOff, off - prevOff);
			prevOff = nameAttr.find_first_not_of(delim, off);
			off = nameAttr.find_first_of(delim, prevOff);

			ThemeView& view = mViews.insert(std::pair<std::string, ThemeView>(viewKey, ThemeView())).first->second;
			parseView(node, view);
		}
	}
}

void parseElement(const pugi::xml_node& root, const std::map<std::string, ElementPropertyType>& typeMap, ThemeData::ThemeElement& element, const std::deque<boost::filesystem::path>& mPaths);

void ThemeData::parseView(const pugi::xml_node& root, ThemeView& view)
{
	;

	for (pugi::xml_node node = root.first_child(); node; node = node.next_sibling())
	{
		if (!node.attribute("name"))
			throw ThemeException(mPaths) << "Element of type \"" << node.name() << "\" missing \"name\" attribute!";

		auto elemTypeIt = sElementMap.find(node.name());
		if (elemTypeIt == sElementMap.end())
			throw ThemeException(mPaths) << "Unknown element of type \"" << node.name() << "\"!";

		const char* delim = " \t\r\n,";
		const std::string nameAttr = node.attribute("name").as_string();
		size_t prevOff = nameAttr.find_first_not_of(delim, 0);
		size_t off = nameAttr.find_first_of(delim, prevOff);
		while (off != std::string::npos || prevOff != std::string::npos)
		{
			std::string elemKey = nameAttr.substr(prevOff, off - prevOff);
			prevOff = nameAttr.find_first_not_of(delim, off);
			off = nameAttr.find_first_of(delim, prevOff);

			parseElement(node, elemTypeIt->second, view.elements.insert(std::pair<std::string, ThemeElement>(elemKey, ThemeElement())).first->second, mPaths);

			if (std::find(view.orderedKeys.begin(), view.orderedKeys.end(), elemKey) == view.orderedKeys.end())
				view.orderedKeys.push_back(elemKey);
		}
	}
}

void parseElement(const pugi::xml_node& root, const std::map<std::string, ElementPropertyType>& typeMap, ThemeData::ThemeElement& element, const std::deque<boost::filesystem::path>& mPaths)
{
	struct Local
	{
		static unsigned int getHexColor(const char* str)
		{
			if (!str)
				throw ThemeException() << "Empty color";

			const size_t len = strlen(str);
			if (len != 6 && len != 8)
				throw ThemeException() << "Invalid color (bad length, \"" << str << "\" - must be 6 or 8)";

			unsigned int val;
			std::stringstream ss;
			ss << str;
			ss >> std::hex >> val;

			if (len == 6)
				val = (val << 8) | 0xFF;

			return val;
		}
	};

	ThemeException error(mPaths);

	element.type = root.name();
	element.extra = root.attribute("extra").as_bool(false);

	for (pugi::xml_node node = root.first_child(); node; node = node.next_sibling())
	{
		auto typeIt = typeMap.find(node.name());
		if (typeIt == typeMap.end())
			throw error << "Unknown property type \"" << node.name() << "\" (for element of type " << root.name() << ").";

		switch (typeIt->second)
		{
		case NORMALIZED_PAIR:
		{
			const std::string str = std::string(node.text().as_string());
			const size_t divider = str.find(' ');
			if (divider == std::string::npos)
				throw error << "invalid normalized pair (property \"" << node.name() << "\", value \"" << str.c_str() << "\")";

			const std::string first = str.substr(0, divider);
			const std::string second = str.substr(divider, std::string::npos);

			element.properties[node.name()] = Eigen::Vector2f(atof(first.c_str()), atof(second.c_str()));
			break;
		}
		case EP_STRING:
			element.properties[node.name()] = std::string(node.text().as_string());
			break;
		case EP_PATH:
		{
			const std::string path = resolvePath(node.text().as_string(), mPaths.back().string());
			if (!ResourceManager::getInstance()->fileExists(path))
			{
				std::stringstream ss;
				ss << "  Warning " << error.what(); // "from theme yadda yadda, included file yadda yadda
				ss << "could not find file \"" << node.text().get() << "\" ";
				if (node.text().get() != path)
					ss << "(which resolved to \"" << path << "\") ";
				LOG(LogWarning) << ss.str();
			}
			element.properties[node.name()] = path;
			break;
		}
		case EP_COLOR:
			element.properties[node.name()] = Local::getHexColor(node.text().as_string());
			break;
		case EP_FLOAT:
			element.properties[node.name()] = node.text().as_float();
			break;
		case EP_BOOLEAN:
			element.properties[node.name()] = node.text().as_bool();
			break;
		default:
			throw error << "Unknown ElementPropertyType for \"" << root.attribute("name").as_string() << "\", property " << node.name();
		}
	}
}

const ThemeData::ThemeElement* ThemeData::getElement(const std::string& view, const std::string& element, const std::string& expectedType) const
{
	auto viewIt = mViews.find(view);
	if (viewIt == mViews.end())
		return nullptr; // not found

	auto elemIt = viewIt->second.elements.find(element);
	if (elemIt == viewIt->second.elements.end())
		return nullptr;

	if (elemIt->second.type != expectedType && !expectedType.empty())
	{
		LOG(LogWarning) << " requested mismatched theme type for [" << view << "." << element << "] - expected \"" << expectedType << "\", got \""
						<< elemIt->second.type << "\"";
		return nullptr;
	}

	return &elemIt->second;
}

const std::shared_ptr<ThemeData>& ThemeData::getDefault()
{
	static std::shared_ptr<ThemeData> theme = nullptr;

	if (theme == nullptr)
	{
		theme = std::shared_ptr<ThemeData>(new ThemeData());

		const std::string path = Platform::getHomePath() + "/.emulationstation/es_theme_default.xml";
		if (fs::exists(path))
		{
			try
			{
				theme->loadFile(path);
			}
			catch (ThemeException& e)
			{
				LOG(LogError) << e.what();
				theme = std::shared_ptr<ThemeData>(new ThemeData()); // reset to empty
			}
		}
	}

	return theme;
}

std::vector<GuiComponent*> ThemeData::makeExtras(const std::shared_ptr<ThemeData>& theme, const std::string& view, Window* window)
{
	std::vector<GuiComponent*> comps;

	auto viewIt = theme->mViews.find(view);
	if (viewIt == theme->mViews.end())
		return comps;

	for (const auto& it : viewIt->second.orderedKeys)
	{
		const ThemeElement& elem = viewIt->second.elements.at(it);
		if (elem.extra)
		{
			GuiComponent* comp = NULL;
			const std::string& et = elem.type;
			if (et == "image")
				comp = new ImageComponent(window);
			else if (et == "text")
				comp = new TextComponent(window);

			comp->applyTheme(theme, view, it, ThemeFlags::ALL);
			comps.push_back(comp);
		}
	}

	return comps;
}

ThemeExtras::ThemeExtras(Window* window)
	: GuiComponent(window)
{
}

ThemeExtras::~ThemeExtras()
{
	for (auto it : mExtras)
		delete it;
}

void ThemeExtras::setExtras(const std::vector<GuiComponent*>& extras)
{
	for (auto it : mExtras) // delete old extras (if any)
		delete it;

	mExtras = extras;
	for (auto it : mExtras)
		addChild(it);
}

std::map<std::string, ThemeSet> ThemeData::getThemeSets()
{
	std::map<std::string, ThemeSet> sets;

	const std::vector<fs::path> paths = {"/etc/emulationstation/themes", Platform::getHomePath() + "/.emulationstation/themes"};

	for (const auto& path : paths)
	{
		if (fs::is_directory(path))
		{
			const fs::directory_iterator end;
			for (fs::directory_iterator it(path); it != end; ++it)
			{
				if (fs::is_directory(*it))
				{
					const ThemeSet set = {*it};
					sets[set.getName()] = set;
				}
			}
		}
	}

	return sets;
}

fs::path ThemeData::getThemeFromCurrentSet(const std::string& system)
{
	auto themeSets = ThemeData::getThemeSets();
	if (themeSets.empty())
		return fs::path(); // no theme sets available

	auto set = themeSets.find(Settings::getInstance()->getString("ThemeSet"));
	if (set == themeSets.end())
	{
		// currently selected theme set is missing, so just pick the first available set
		set = themeSets.begin();
		Settings::getInstance()->setString("ThemeSet", set->first);
	}

	return set->second.getThemePath(system);
}

#if defined(EXTENSION)
bool ThemeData::getHasFavoritesInTheme() const
{
	return (mVersion >= CURRENT_THEME_FORMAT_VERSION);
}
#endif
