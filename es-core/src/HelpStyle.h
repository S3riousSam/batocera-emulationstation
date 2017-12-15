#pragma once
#include <Eigen/Dense>
#include <memory>
#include <string>

class ThemeData;
class Font;

struct HelpStyle
{
	HelpStyle();
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view);

	Eigen::Vector2f position;
	unsigned int iconColor;
	unsigned int textColor;
	std::shared_ptr<Font> font;
};