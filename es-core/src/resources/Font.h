#pragma once
#include "platform.h"
#include GLHEADER
#include <ft2build.h>
#include <string>
#include "ThemeData.h"
#include "resources/ResourceManager.h"
#include <Eigen/Dense>

typedef struct FT_FaceRec_*  FT_Face;

class TextCache;

#define FONT_SIZE_SMALL (static_cast<unsigned int>(0.035f * Renderer::getScreenHeight()))
#define FONT_SIZE_MEDIUM (static_cast<unsigned int>(0.045f * Renderer::getScreenHeight()))
#define FONT_SIZE_LARGE (static_cast<unsigned int>(0.085f * Renderer::getScreenHeight()))

#define FONT_PATH_LIGHT ":/ubuntu_condensed.ttf"
#define FONT_PATH_REGULAR ":/ubuntu_condensed.ttf"

typedef unsigned long UnicodeChar;

enum Alignment
{
	ALIGN_LEFT,
	ALIGN_CENTER, // centers both horizontally and vertically
	ALIGN_RIGHT
};

// Renders TrueType font using FreeType and OpenGL.
// The library is automatically initialized when it's needed.
class Font : public IReloadable
{
public:
	virtual ~Font();

	static void uinitLibrary();

	static std::shared_ptr<Font> get(int size, const std::string& path = getDefaultPath());

	Eigen::Vector2f sizeText(const std::string& text, float lineSpacing = 1.5f); // Returns the expected size of a string when rendered.  Extra spacing is applied to the Y axis.
	TextCache* buildTextCache(const std::string& text, float offsetX, float offsetY, unsigned int color);
	TextCache* buildTextCache(
		const std::string& text, Eigen::Vector2f offset, unsigned int color, float xLen, Alignment alignment = ALIGN_LEFT, float lineSpacing = 1.5f);
	void renderTextCache(TextCache* cache);

	// Inserts newlines into text to make it wrap properly.
	std::string wrapText(std::string text, float xLen);

	// Returns the expected size of a string after wrapping is applied.
	Eigen::Vector2f sizeWrappedText(const std::string& text, float xLen, float lineSpacing = 1.5f);

	// Returns the position of of the cursor after moving "cursor" characters.
	Eigen::Vector2f getWrappedTextCursorOffset(const std::string& text, float xLen, size_t cursor, float lineSpacing = 1.5f);

	float getHeight(float lineSpacing = 1.5f) const;
	float getLetterHeight();

	void unload(std::shared_ptr<ResourceManager>& rm) override;
	void reload(std::shared_ptr<ResourceManager>& rm) override;

	int getSize() const;
	const std::string& getPath() const
	{
		return mPath;
	}

	static const char* getDefaultPath()
	{
		return FONT_PATH_REGULAR;
	}

	static std::shared_ptr<Font> getFromTheme(const ThemeData::ThemeElement* elem, unsigned int properties, const std::shared_ptr<Font>& orig);

	size_t getMemUsage() const; // returns an approximation of VRAM used by this font's texture (in bytes)
	static size_t getTotalMemUsage(); // returns an approximation of total VRAM used by font textures (in bytes)

	// utf8 stuff
	static size_t getNextCursor(const std::string& str, size_t cursor);
	static size_t getPrevCursor(const std::string& str, size_t cursor);
	static size_t moveCursor(const std::string& str, size_t cursor, int moveAmt); // negative moveAmt = move backwards, positive = move forwards
	static UnicodeChar readUnicodeChar(const std::string& str, size_t& cursor); // reads unicode character at cursor AND moves cursor to the next valid unicode char

private:
	static std::map<std::pair<std::string, int>, std::weak_ptr<Font>> sFontMap;

	Font(int size, const std::string& path);

	void rebuildTextures();
	void unloadTextures();

	struct FontTexture;
	std::vector<FontTexture> mTextures;

	void getTextureForNewGlyph(const Eigen::Vector2i& glyphSize, FontTexture*& tex_out, Eigen::Vector2i& cursor_out);

	struct FontFace;
	mutable std::map<unsigned int, std::unique_ptr<FontFace>> mFaceCache;
	FT_Face getFaceForChar(UnicodeChar id) const;

	struct Glyph;
	std::map<UnicodeChar, Glyph> mGlyphMap;

	Glyph* getGlyph(UnicodeChar id);
	const Glyph* getGlyph(UnicodeChar id) const;

	int mMaxGlyphHeight;

	const int mSize;
	const std::string mPath;

	float getNewlineStartOffset(const std::string& text, const unsigned int& charStart, const float& xLen, const Alignment& alignment);

	friend TextCache;
};

// Used to store a sort of "pre-rendered" string.
// When a TextCache is constructed (Font::buildTextCache()), the vertices and texture coordinates of the string are calculated and stored in the
// TextCache object. Rendering a previously constructed TextCache (Font::renderTextCache) every frame is MUCH faster than rebuilding one every frame.
// Keep in mind you still need the Font object to render a TextCache (as the Font holds the OpenGL texture), and if a Font changes your TextCache may
// become invalid.
class TextCache
{
protected:
	struct Vertex
	{
		Eigen::Vector2f pos;
		Eigen::Vector2f tex;
	};

	struct VertexList
	{
		GLuint* textureIdPtr; // this is a pointer because the texture ID can change during deinit/reinit (when launching a game)
		std::vector<Vertex> verts;
		std::vector<GLubyte> colors;
	};

	std::vector<VertexList> vertexLists;

public:
	struct CacheMetrics
	{
		Eigen::Vector2f size;
	} metrics;

	void setColor(unsigned int color);

	friend Font;
};
