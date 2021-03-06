#include "components/ComponentGrid.h"
#include "LocaleES.h"
#include "Log.h"
#include "Renderer.h"
#include "Settings.h"

ComponentGrid::ComponentGrid(Window* window, const Eigen::Vector2i& gridDimensions)
	: GuiComponent(window)
	, mGridSize(gridDimensions)
	, mCursor(0, 0)
	, mColWidths(new float[gridDimensions.x()])
	, mRowHeights(new float[gridDimensions.y()])
{
	assert(gridDimensions.x() > 0 && gridDimensions.y() > 0);

	mCells.reserve(gridDimensions.x() * gridDimensions.y());

	for (int x = 0; x < gridDimensions.x(); x++)
		mColWidths[x] = 0;
	for (int y = 0; y < gridDimensions.y(); y++)
		mRowHeights[y] = 0;
}

ComponentGrid::~ComponentGrid()
{
	delete[] mRowHeights;
	delete[] mColWidths;
	mRowHeights = nullptr; // to help debugging
	mColWidths = nullptr; // to help debugging
}

float ComponentGrid::getColWidth(int col) const
{
	if (mColWidths[col] != 0)
		return mColWidths[col] * mSize.x();

	// calculate automatic width
	float freeWidthPerc = 1;
	int between = 0;
	for (int x = 0; x < mGridSize.x(); x++)
	{
		freeWidthPerc -= mColWidths[x]; // if it's 0 it won't do anything
		if (mColWidths[x] == 0)
			between++;
	}

	return (freeWidthPerc * mSize.x()) / between;
}

float ComponentGrid::getRowHeight(int row) const
{
	if (mRowHeights[row] != 0)
		return mRowHeights[row] * mSize.y();

	// calculate automatic height
	float freeHeightPerc = 1;
	int between = 0;
	for (int y = 0; y < mGridSize.y(); y++)
	{
		freeHeightPerc -= mRowHeights[y]; // if it's 0 it won't do anything
		if (mRowHeights[y] == 0)
			between++;
	}

	return (freeHeightPerc * mSize.y()) / between;
}

void ComponentGrid::setColWidthPerc(int col, float width, bool update)
{
	assert(width >= 0 && width <= 1);
	assert(col >= 0 && col < mGridSize.x());
	mColWidths[col] = width;

	if (update)
		onSizeChanged();
}

void ComponentGrid::setRowHeightPerc(int row, float height, bool update)
{
	assert(height >= 0 && height <= 1);
	assert(row >= 0 && row < mGridSize.y());
	mRowHeights[row] = height;

	if (update)
		onSizeChanged();
}

void ComponentGrid::setEntry(const std::shared_ptr<GuiComponent>& comp, const Eigen::Vector2i& pos, bool canFocus, bool resize,
	const Eigen::Vector2i& size, unsigned int border, GridFlags::UpdateType updateType)
{
	assert(pos.x() >= 0 && pos.x() < mGridSize.x() && pos.y() >= 0 && pos.y() < mGridSize.y());
	assert(comp != nullptr);
	assert(comp->getParent() == NULL);

	GridEntry entry(pos, size, comp, canFocus, resize, updateType, border);
	mCells.push_back(entry);

	addChild(comp.get());

	if (!cursorValid() && canFocus)
	{
		auto origCursor = mCursor;
		mCursor = pos;
		onCursorMoved(origCursor, mCursor);
	}

	updateCellComponent(mCells.back());
	updateSeparators();
}

bool ComponentGrid::removeEntry(const std::shared_ptr<GuiComponent>& comp)
{
	for (auto it = mCells.begin(); it != mCells.end(); it++)
	{
		if (it->component == comp)
		{
			removeChild(comp.get());
			mCells.erase(it);
			return true;
		}
	}

	return false;
}

void ComponentGrid::updateCellComponent(const GridEntry& cell)
{
	// size
	Eigen::Vector2f size(0, 0);
	for (int x = cell.pos.x(); x < cell.pos.x() + cell.dim.x(); x++)
		size[0] += getColWidth(x);
	for (int y = cell.pos.y(); y < cell.pos.y() + cell.dim.y(); y++)
		size[1] += getRowHeight(y);

	if (cell.resize)
		cell.component->setSize(size);

	// position
	// find top left corner
	Eigen::Vector3f pos(0, 0, 0);
	for (int x = 0; x < cell.pos.x(); x++)
		pos[0] += getColWidth(x);
	for (int y = 0; y < cell.pos.y(); y++)
		pos[1] += getRowHeight(y);

	// center component
	pos[0] = pos.x() + (size.x() - cell.component->getSize().x()) / 2;
	pos[1] = pos.y() + (size.y() - cell.component->getSize().y()) / 2;

	cell.component->setPosition(pos);
}

void ComponentGrid::updateSeparators()
{
	mLines.clear();

	bool drawAll = Settings::getInstance()->getBool("DebugGrid");

	Eigen::Vector2f pos;
	Eigen::Vector2f size;
	for (const auto& it : mCells)
	{
		if (!it.border && !drawAll)
			continue;

		// find component position + size
		pos << 0, 0;
		size << 0, 0;
		for (int x = 0; x < it.pos.x(); x++)
			pos[0] += getColWidth(x);
		for (int y = 0; y < it.pos.y(); y++)
			pos[1] += getRowHeight(y);
		for (int x = it.pos.x(); x < it.pos.x() + it.dim.x(); x++)
			size[0] += getColWidth(x);
		for (int y = it.pos.y(); y < it.pos.y() + it.dim.y(); y++)
			size[1] += getRowHeight(y);

		if (it.border & GridFlags::BORDER_TOP || drawAll)
		{
			mLines.push_back(Vert(pos.x(), pos.y()));
			mLines.push_back(Vert(pos.x() + size.x(), pos.y()));
		}
		if (it.border & GridFlags::BORDER_BOTTOM || drawAll)
		{
			mLines.push_back(Vert(pos.x(), pos.y() + size.y()));
			mLines.push_back(Vert(pos.x() + size.x(), mLines.back().y));
		}
		if (it.border & GridFlags::BORDER_LEFT || drawAll)
		{
			mLines.push_back(Vert(pos.x(), pos.y()));
			mLines.push_back(Vert(pos.x(), pos.y() + size.y()));
		}
		if (it.border & GridFlags::BORDER_RIGHT || drawAll)
		{
			mLines.push_back(Vert(pos.x() + size.x(), pos.y()));
			mLines.push_back(Vert(mLines.back().x, pos.y() + size.y()));
		}
	}

	mLineColors.reserve(mLines.size());
	Renderer::buildGLColorArray((GLubyte*)mLineColors.data(), 0xC6C7C6FF, mLines.size());
}

void ComponentGrid::onSizeChanged()
{
	for (auto it : mCells)
		updateCellComponent(it);

	updateSeparators();
}

ComponentGrid::GridEntry* ComponentGrid::getCellAt(int x, int y)
{
	assert(x >= 0 && x < mGridSize.x() && y >= 0 && y < mGridSize.y());

	for (auto& it : mCells)
	{
		const int xmin = it.pos.x();
		const int xmax = xmin + it.dim.x();
		const int ymin = it.pos.y();
		const int ymax = ymin + it.dim.y();

		if (x >= xmin && y >= ymin && x < xmax && y < ymax)
			return &it;
	}

	return nullptr;
}

const ComponentGrid::GridEntry* ComponentGrid::getCellAt(int x, int y) const
{
	assert(x >= 0 && x < mGridSize.x() && y >= 0 && y < mGridSize.y());

	for (const auto& it : mCells)
	{
		const int xmin = it.pos.x();
		const int xmax = xmin + it.dim.x();
		const int ymin = it.pos.y();
		const int ymax = ymin + it.dim.y();

		if (x >= xmin && y >= ymin && x < xmax && y < ymax)
			return &it;
	}

	return nullptr;
}

bool ComponentGrid::input(InputConfig* config, Input input)
{
	GridEntry* cursorEntry = getCellAt(mCursor);
	if (cursorEntry != nullptr && cursorEntry->component->input(config, input))
		return true;

	if (!input.value)
		return false;

	if (config->isMappedTo("down", input))
		return moveCursor(Eigen::Vector2i(0, 1));
	if (config->isMappedTo("up", input))
		return moveCursor(Eigen::Vector2i(0, -1));
	if (config->isMappedTo("left", input))
		return moveCursor(Eigen::Vector2i(-1, 0));
	if (config->isMappedTo("right", input))
		return moveCursor(Eigen::Vector2i(1, 0));

	return false;
}

void ComponentGrid::resetCursor()
{
	if (!mCells.size())
		return;

	for (const auto& it : mCells)
	{
		if (it.canFocus)
		{
			const Eigen::Vector2i origCursor = mCursor;
			mCursor = it.pos;
			onCursorMoved(origCursor, mCursor);
			break;
		}
	}
}

bool ComponentGrid::moveCursor(Eigen::Vector2i dir)
{
	assert(dir.x() || dir.y());

	const Eigen::Vector2i origCursor = mCursor;

	const GridEntry* currentCursorEntry = getCellAt(mCursor);

	Eigen::Vector2i searchAxis(dir.x() == 0, dir.y() == 0);

	while (mCursor.x() >= 0 && mCursor.y() >= 0 && mCursor.x() < mGridSize.x() && mCursor.y() < mGridSize.y())
	{
		mCursor = mCursor + dir;

		const Eigen::Vector2i curDirPos = mCursor;

		// spread out on search axis+
		while (mCursor.x() < mGridSize.x() && mCursor.y() < mGridSize.y() && mCursor.x() >= 0 && mCursor.y() >= 0)
		{
			const GridEntry* cursorEntry = getCellAt(mCursor);
			if (cursorEntry && cursorEntry->canFocus && cursorEntry != currentCursorEntry)
			{
				onCursorMoved(origCursor, mCursor);
				return true;
			}

			mCursor += searchAxis;
		}

		// now again on search axis-
		mCursor = curDirPos;
		while (mCursor.x() >= 0 && mCursor.y() >= 0 && mCursor.x() < mGridSize.x() && mCursor.y() < mGridSize.y())
		{
			const GridEntry* cursorEntry = getCellAt(mCursor);
			if (cursorEntry && cursorEntry->canFocus && cursorEntry != currentCursorEntry)
			{
				onCursorMoved(origCursor, mCursor);
				return true;
			}

			mCursor -= searchAxis;
		}

		mCursor = curDirPos;
	}

	// failed to find another focusable element in this direction
	mCursor = origCursor;
	return false;
}

void ComponentGrid::onFocusLost()
{
	GridEntry* cursorEntry = getCellAt(mCursor);
	if (cursorEntry != nullptr)
		cursorEntry->component->onFocusLost();
}

void ComponentGrid::onFocusGained()
{
	GridEntry* cursorEntry = getCellAt(mCursor);
	if (cursorEntry != nullptr)
		cursorEntry->component->onFocusGained();
}

bool ComponentGrid::cursorValid()
{
	GridEntry* e = getCellAt(mCursor);
	return (e != nullptr && e->canFocus);
}

void ComponentGrid::update(int deltaTime)
{
	// update ALL THE THINGS
	GridEntry* cursorEntry = getCellAt(mCursor);
	for (auto& it : mCells)
	{
		if (it.updateType == GridFlags::UpdateType::Always || (it.updateType == GridFlags::UpdateType::WhenSelected && cursorEntry == &it))
			it.component->update(deltaTime);
	}
}

void ComponentGrid::render(const Eigen::Affine3f& parentTrans)
{
	const Eigen::Affine3f trans = parentTrans * getTransform();

	renderChildren(trans);

	// draw cell separators
	if (mLines.size())
	{
		Renderer::setMatrix(trans);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glVertexPointer(2, GL_FLOAT, 0, &mLines[0].x);
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, mLineColors.data());

		glDrawArrays(GL_LINES, 0, mLines.size());

		glDisable(GL_BLEND);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}

void ComponentGrid::textInput(const char* text)
{
	GridEntry* selectedEntry = getCellAt(mCursor);
	if (selectedEntry != nullptr && selectedEntry->canFocus)
		selectedEntry->component->textInput(text);
}

void ComponentGrid::onCursorMoved(const Eigen::Vector2i& from, const Eigen::Vector2i& to)
{
	{
		GridEntry* cell = getCellAt(from);
		if (cell != nullptr)
			cell->component->onFocusLost();
	}
	{
		GridEntry* cell = getCellAt(to);
		if (cell != nullptr)
			cell->component->onFocusGained();
	}

	updateHelpPrompts();
}

void ComponentGrid::setCursorTo(const std::shared_ptr<GuiComponent>& comp)
{
	for (const auto& it : mCells)
	{
		if (it.component == comp)
		{
			const Eigen::Vector2i oldCursor = mCursor;
			mCursor = it.pos;
			onCursorMoved(oldCursor, mCursor);
			return;
		}
	}

	assert(false); // Component not found!
}

std::shared_ptr<GuiComponent> ComponentGrid::getSelectedComponent() const
{
	const GridEntry* e = getCellAt(mCursor);
	return (e != nullptr) ? e->component : nullptr;
}

std::vector<HelpPrompt> ComponentGrid::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = (getCellAt(mCursor) != nullptr) ? getCellAt(mCursor)->component->getHelpPrompts() : std::vector<HelpPrompt>();

	bool canScrollVert = mGridSize.y() > 1;
	bool canScrollHoriz = mGridSize.x() > 1;
	for (const auto& it : prompts)
	{
		if (it.first == "up/down/left/right")
		{
			canScrollHoriz = false;
			canScrollVert = false;
			break;
		}
		else if (it.first == "up/down")
		{
			canScrollVert = false;
		}
		else if (it.first == "left/right")
		{
			canScrollHoriz = false;
		}
	}

	if (canScrollHoriz && canScrollVert)
		prompts.push_back(HelpPrompt("up/down/left/right", _("CHOOSE")));
	else if (canScrollHoriz)
		prompts.push_back(HelpPrompt("left/right", _("CHOOSE")));
	else if (canScrollVert)
		prompts.push_back(HelpPrompt("up/down", _("CHOOSE")));

	return prompts;
}
