#pragma once
#include "GuiComponent.h"
#include "pugixml/pugixml.hpp"
#include <Eigen/Dense>
#include <boost/filesystem.hpp>
#include <boost/variant.hpp>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

class Window;

namespace ThemeFlags
{
	enum PropertyFlags : unsigned int
	{
		PATH = 1,
		POSITION = 2,
		SIZE = 4,
		ORIGIN = 8,
		COLOR = 16,
		FONT_PATH = 32,
		FONT_SIZE = 64,
		SOUND = 128,
		ALIGNMENT = 256,
		TEXT = 512,
		FORCE_UPPERCASE = 1024,
		LINE_SPACING = 2048,

		ALL = 0xFFFFFFFF
	};
}

class ThemeException : public std::exception
{
public:
	ThemeException() = default;
	ThemeException(const std::deque<boost::filesystem::path>& deque)
	{
		*this << "from theme \"" << deque.front().string() << "\"\n";
		for (auto it = deque.begin() + 1; it != deque.end(); it++)
			*this << "  (from included file \"" << (*it).string() << "\")\n";
		*this << "    ";
	}

	const char* what() const throw() override
	{
		return msg.c_str();
	}

	template<typename T>
	friend ThemeException& operator<<(ThemeException& e, T msg);

	template<typename T>
	friend ThemeException operator<<(const ThemeException& e, T msg);

private:
	std::string msg;
};

template<typename T>
ThemeException& operator<<(ThemeException& e, T appendMsg)
{
	std::stringstream ss;
	ss << e.msg << appendMsg;
	e.msg = ss.str();
	return e;
}

template<typename T>
ThemeException operator<<(const ThemeException& e, T appendMsg)
{
	std::stringstream ss;
	ss << e.msg << appendMsg;
	ThemeException result(e);
	result.msg = ss.str();
	return result;
}

class ThemeExtras : public GuiComponent
{
public:
	ThemeExtras(Window* window);
	virtual ~ThemeExtras();

	// This method takes ownership of the components within extras
	// it deletes them in destructor or when setExtras is called again.
	void setExtras(const std::vector<GuiComponent*>& extras);

private:
	std::vector<GuiComponent*> mExtras;
};

struct ThemeSet
{
	boost::filesystem::path path;

	std::string getName() const
	{
		return path.stem().string();
	}
	boost::filesystem::path getThemePath(const std::string& system) const
	{
		return path / system / "theme.xml";
	}
};

class ThemeData
{
public:
	class ThemeElement
	{
	public:
		bool extra;
		std::string type;

		std::map<std::string, boost::variant<Eigen::Vector2f, std::string, unsigned int, float, bool>> properties;

		template<typename T>
		T get(const std::string& prop) const
		{
			return boost::get<T>(properties.at(prop));
		}

		bool has(const std::string& prop) const
		{
			return (properties.find(prop) != properties.end());
		}
	};

private:
	class ThemeView
	{
	public:
		std::map<std::string, ThemeElement> elements;
		std::vector<std::string> orderedKeys;
	};

public:
	ThemeData();

	void loadFile(const std::string& path); // throws ThemeException

	// If expectedType is an empty string, will do no type checking.
	const ThemeElement* getElement(const std::string& view, const std::string& element, const std::string& expectedType) const;

	static std::vector<GuiComponent*> makeExtras(const std::shared_ptr<ThemeData>& theme, const std::string& view, Window* window);

	static const std::shared_ptr<ThemeData>& getDefault();

	static std::map<std::string, ThemeSet> getThemeSets();
	static boost::filesystem::path getThemeFromCurrentSet(const std::string& system);
#if defined(EXTENSION)
	bool getHasFavoritesInTheme() const;
#endif

private:
	std::deque<boost::filesystem::path> mPaths;
	float mVersion;

	void parseIncludes(const pugi::xml_node& themeRoot);
	void parseViews(const pugi::xml_node& themeRoot);
	void parseView(const pugi::xml_node& viewNode, ThemeView& view);

	std::map<std::string, ThemeView> mViews;
};
