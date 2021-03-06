#include "HelpStyle.h"
#include "Renderer.h"
#include "ThemeData.h"
#include "resources/Font.h"

HelpStyle::HelpStyle()
    : position(Eigen::Vector2f(Renderer::getScreenWidth() * 0.012f, Renderer::getScreenHeight() * 0.9515f))
    , iconColor(COLOR_GRAY3)
    , textColor(COLOR_GRAY3)
    , font((FONT_SIZE_SMALL != 0) ? Font::get(FONT_SIZE_SMALL) : nullptr)
{
}

void HelpStyle::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view)
{
	auto elem = theme->getElement(view, "help", "helpsystem");
	if (elem == nullptr)
		return;

	if (elem->has("pos"))
	{
		position = elem->get<Eigen::Vector2f>("pos").cwiseProduct(Eigen::Vector2f(
			static_cast<float>(Renderer::getScreenWidth()),
			static_cast<float>(Renderer::getScreenHeight())));
	}

	if (elem->has("textColor"))
		textColor = elem->get<unsigned int>("textColor");

	if (elem->has("iconColor"))
		iconColor = elem->get<unsigned int>("iconColor");

	if (elem->has("fontPath") || elem->has("fontSize"))
		font = Font::getFromTheme(elem, ThemeFlags::ALL, font);
}
