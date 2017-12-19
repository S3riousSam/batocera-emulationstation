#pragma once
#include "GuiComponent.h"

namespace GridFlags
{
	enum class UpdateType
	{
		Always,
		WhenSelected,
		//NeverUpdate
	};

	enum Border : unsigned int
	{
		BORDER_NONE = 0,

		BORDER_TOP = 1,
		BORDER_BOTTOM = 2,
		BORDER_LEFT = 4,
		BORDER_RIGHT = 8
	};
}; // namespace GridFlags

// Arranges components in a spreadsheet-style grid.
class ComponentGrid : public GuiComponent
{
public:
	ComponentGrid(Window* window, const Eigen::Vector2i& gridDimensions);
	virtual ~ComponentGrid();

	bool removeEntry(const std::shared_ptr<GuiComponent>& comp);

	void setEntry(const std::shared_ptr<GuiComponent>& comp, const Eigen::Vector2i& pos, bool canFocus, bool resize = true,
		const Eigen::Vector2i& size = Eigen::Vector2i(1, 1), unsigned int border = GridFlags::BORDER_NONE,
		GridFlags::UpdateType updateType = GridFlags::UpdateType::Always);

	void textInput(const char* text) override;
	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Eigen::Affine3f& parentTrans) override;
	void onSizeChanged() override;

	void resetCursor();
	bool cursorValid();

	float getColWidth(int col) const;
	float getRowHeight(int row) const;

	// if update is false, will not call an onSizeChanged() which triggers a (potentially costly) repositioning + resizing of every element
	void setColWidthPerc(int col, float width, bool update = true);
	void setRowHeightPerc(int row, float height, bool update = true);

	bool moveCursor(Eigen::Vector2i dir);
	void setCursorTo(const std::shared_ptr<GuiComponent>& comp);

	std::shared_ptr<GuiComponent> getSelectedComponent() const;

	void onFocusLost() override;
	void onFocusGained() override;

	virtual std::vector<HelpPrompt> getHelpPrompts() override;

private:
	class GridEntry
	{
	public:
		Eigen::Vector2i pos;
		Eigen::Vector2i dim;
		std::shared_ptr<GuiComponent> component;
		bool canFocus;
		bool resize;
		GridFlags::UpdateType updateType;
		unsigned int border;

		GridEntry(const Eigen::Vector2i& p = Eigen::Vector2i::Zero(), const Eigen::Vector2i& d = Eigen::Vector2i::Zero(),
			const std::shared_ptr<GuiComponent>& cmp = nullptr, bool f = false, bool r = true,
			GridFlags::UpdateType u = GridFlags::UpdateType::Always, unsigned int b = GridFlags::BORDER_NONE)
			: pos(p)
			, dim(d)
			, component(cmp)
			, canFocus(f)
			, resize(r)
			, updateType(u)
			, border(b)
		{
		}

		operator bool() const
		{
			return component != nullptr;
		}
	};

	float* mRowHeights;
	float* mColWidths;

	struct Vert
	{
		Vert(float xi = 0, float yi = 0)
			: x(xi)
			, y(yi)
		{
		}
		float x;
		float y;
	};

	std::vector<Vert> mLines;
	std::vector<unsigned int> mLineColors;

	// Update position & size
	void updateCellComponent(const GridEntry& cell);
	void updateSeparators();

	GridEntry* getCellAt(int x, int y);
	const GridEntry* getCellAt(int x, int y) const;
	inline GridEntry* getCellAt(const Eigen::Vector2i& pos)
	{
		return getCellAt(pos.x(), pos.y());
	}

	inline const GridEntry* getCellAt(const Eigen::Vector2i& pos) const
	{
		return getCellAt(pos.x(), pos.y());
	}

	Eigen::Vector2i mGridSize;

	std::vector<GridEntry> mCells;

	void onCursorMoved(const Eigen::Vector2i& from, const Eigen::Vector2i& to);
	Eigen::Vector2i mCursor;
};
