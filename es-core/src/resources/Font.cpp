#include "resources/Font.h"
#include "Log.h"
#include "Renderer.h"
#include "Util.h"
#include FT_FREETYPE_H
//#include <algorithm>
//#include <iostream>
//#include <vector>

namespace
{
	static FT_Library sLibrary = nullptr;

	void initLibrary()
	{
		assert(sLibrary == nullptr);
		if (FT_Init_FreeType(&sLibrary) != 0) // TODO: Memory leak (never calling FT_Done_Face(&sLibrary))!??
		{
			sLibrary = nullptr;
			LOG(LogError) << "Error initializing FreeType!";
		}
	}

    float font_round(float v)
    {
        return round(v);
    }
}

struct Font::FontTexture
{
	FontTexture();
	~FontTexture();

	bool findEmpty(const Eigen::Vector2i& size, Eigen::Vector2i& cursor_out);

	// you must call initTexture() after creating a FontTexture to get a textureId
	void initTexture(); // initializes the OpenGL texture according to this FontTexture's settings, updating textureId
	void deinitTexture(); // deinitializes the OpenGL texture if any exists, is automatically called in the destructor

	GLuint textureId;
	Eigen::Vector2i textureSize;

	Eigen::Vector2i writePos;
	int rowHeight;
};

struct Font::FontFace
{
	FontFace(ResourceData&& d, int size);
	virtual ~FontFace();

	const ResourceData data;
	FT_Face face;
};

struct Font::Glyph
{
	FontTexture* texture;

	Eigen::Vector2f texPos;
	Eigen::Vector2f texSize; // in texels!

	Eigen::Vector2f advance;
	Eigen::Vector2f bearing;
};

int Font::getSize() const
{
	return mSize;
}

std::map<std::pair<std::string, int>, std::weak_ptr<Font>> Font::sFontMap;

// utf8 stuff
size_t Font::getNextCursor(const std::string& str, size_t cursor)
{
	// compare to character at the cursor
	const char& c = str[cursor];

	size_t result = cursor;
	if ((c & 0x80) == 0) // 0xxxxxxx, one byte character
	{
		result += 1;
	}
	else if ((c & 0xE0) == 0xC0) // 110xxxxx, two bytes left in character
	{
		result += 2;
	}
	else if ((c & 0xF0) == 0xE0) // 1110xxxx, three bytes left in character
	{
		result += 3;
	}
	else if ((c & 0xF8) == 0xF0) // 11110xxx, four bytes left in character
	{
		result += 4;
	}
	else
	{
		// error, invalid utf8 string

		// if this assert is tripped, the cursor is in the middle of a utf8 code point
		assert((c & 0xC0) != 0x80); // character is 10xxxxxx

		// if that wasn't it, something crazy happened
		assert(false);
	}

	if (str.length() < result || result < cursor) // don't go beyond the very end of the string, try and catch overflow
		return cursor;
	return result;
}

// note: will happily accept malformed utf8
size_t Font::getPrevCursor(const std::string& str, size_t cursor)
{
	if (cursor == 0)
		return 0;

	do
	{
		cursor--;
	} while (cursor > 0 && (str[cursor] & 0xC0) == 0x80); // character is 10xxxxxx

	return cursor;
}

size_t Font::moveCursor(const std::string& str, size_t cursor, int amt)
{
	if (amt > 0)
	{
		for (int i = 0; i < amt; i++)
			cursor = Font::getNextCursor(str, cursor);
	}
	else if (amt < 0)
	{
		for (int i = amt; i < 0; i++)
			cursor = Font::getPrevCursor(str, cursor);
	}

	return cursor;
}

UnicodeChar Font::readUnicodeChar(const std::string& str, size_t& cursor)
{
	const char& c = str[cursor];

	if ((c & 0x80) == 0) // 0xxxxxxx, one byte character
	{
		// 0xxxxxxx
		cursor++;
		return (UnicodeChar)c;
	}
	else if ((c & 0xE0) == 0xC0) // 110xxxxx, two bytes left in character
	{
		// 110xxxxx 10xxxxxx
		UnicodeChar val = ((str[cursor] & 0x1F) << 6) | (str[cursor + 1] & 0x3F);
		cursor += 2;
		return val;
	}
	else if ((c & 0xF0) == 0xE0) // 1110xxxx, three bytes left in character
	{
		// 1110xxxx 10xxxxxx 10xxxxxx
		UnicodeChar val = ((str[cursor] & 0x0F) << 12) | ((str[cursor + 1] & 0x3F) << 6) | (str[cursor + 2] & 0x3F);
		cursor += 3;
		return val;
	}
	else if ((c & 0xF8) == 0xF0) // 11110xxx, four bytes left in character
	{
		// 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
		UnicodeChar val =
			((str[cursor] & 0x07) << 18) | ((str[cursor + 1] & 0x3F) << 12) | ((str[cursor + 2] & 0x3F) << 6) | (str[cursor + 3] & 0x3F);
		cursor += 4;
		return val;
	}
	else
	{
		// error, invalid utf8 string

		// if this assert is tripped, the cursor is in the middle of a utf8 code point
		assert((c & 0xC0) != 0x80); // character is 10xxxxxx

		// if that wasn't it, something crazy happened
		assert(false);
	}

	// error
	return 0;
}

Font::FontFace::FontFace(ResourceData&& d, int size)
	: data(d)
	, face(nullptr)
{
	assert(sLibrary != nullptr);
	const int err = FT_New_Memory_Face(sLibrary, data.ptr.get(), data.length, 0, &face);
	assert(err == 0);

	FT_Set_Pixel_Sizes(face, 0, size);
}

Font::FontFace::~FontFace()
{
	if (face != nullptr)
	{
		const int err = FT_Done_Face(face);
		assert(err == 0);
	}
}

void Font::uinitLibrary()
{
    assert(sLibrary != nullptr);
    FT_Done_FreeType(sLibrary);
}

size_t Font::getMemUsage() const
{
	size_t memUsage = 0;
	for (const auto& it : mTextures)
		memUsage += it.textureSize.x() * it.textureSize.y() * 4;

	for (const auto& it : mFaceCache)
		memUsage += it.second->data.length;

	return memUsage;
}

size_t Font::getTotalMemUsage()
{
	size_t total = 0;

	auto it = sFontMap.begin();
	while (it != sFontMap.end())
	{
		if (it->second.expired())
		{
			it = sFontMap.erase(it);
			continue;
		}

		total += it->second.lock()->getMemUsage();
		it++;
	}

	return total;
}

Font::Font(int size, const std::string& path)
	: mSize(size)
	, mPath(path)
	, mMaxGlyphHeight{}
{
	assert(mSize > 0);

	if (sLibrary == nullptr)
		initLibrary();

	for (UnicodeChar i = 32; i < 128; i++) // init ASCII characters
		getGlyph(i);

	mFaceCache.clear(); // Required
}

Font::~Font()
{
	unload(ResourceManager::getInstance());
}

void Font::reload(std::shared_ptr<ResourceManager>& rm)
{
	rebuildTextures();
}

void Font::unload(std::shared_ptr<ResourceManager>& rm)
{
	unloadTextures();
}

std::shared_ptr<Font> Font::get(int size, const std::string& path)
{
	const std::string canonicalPath = getCanonicalPath(path);

	const std::pair<std::string, int> def(canonicalPath.empty() ? getDefaultPath() : canonicalPath, size);
	const auto foundFont = sFontMap.find(def);
	if (foundFont != sFontMap.end())
	{
		if (!foundFont->second.expired())
			return foundFont->second.lock();
	}

	std::shared_ptr<Font> font = std::shared_ptr<Font>(new Font(def.second, def.first));
	sFontMap[def] = std::weak_ptr<Font>(font);
	ResourceManager::getInstance()->addReloadable(font);
	return font;
}

void Font::unloadTextures()
{
	for (auto& it : mTextures)
		it.deinitTexture();
}

Font::FontTexture::FontTexture()
{
	textureId = 0;
	textureSize << 2048, 512;
	writePos = Eigen::Vector2i::Zero();
	rowHeight = 0;
}

Font::FontTexture::~FontTexture()
{
	deinitTexture();
}

bool Font::FontTexture::findEmpty(const Eigen::Vector2i& size, Eigen::Vector2i& cursor_out)
{
	if (size.x() >= textureSize.x() || size.y() >= textureSize.y())
		return false;

	if (writePos.x() + size.x() >= textureSize.x() && writePos.y() + rowHeight + size.y() + 1 < textureSize.y())
	{
		// row full, but it should fit on the next row
		// move cursor to next row
		writePos << 0, writePos.y() + rowHeight + 1; // leave 1px of space between glyphs
		rowHeight = 0;
	}

	if (writePos.x() + size.x() >= textureSize.x() || writePos.y() + size.y() >= textureSize.y())
	{
		// nope, still won't fit
		return false;
	}

	cursor_out = writePos;
	writePos[0] += size.x() + 1; // leave 1px of space between glyphs

	if (size.y() > rowHeight)
		rowHeight = size.y();

	return true;
}

void Font::FontTexture::initTexture()
{
	assert(textureId == 0);

	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, textureSize.x(), textureSize.y(), 0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL);
}

void Font::FontTexture::deinitTexture()
{
	if (textureId != 0)
	{
		glDeleteTextures(1, &textureId);
		textureId = 0;
	}
}

void Font::getTextureForNewGlyph(const Eigen::Vector2i& glyphSize, FontTexture*& tex_out, Eigen::Vector2i& cursor_out)
{
	// check if the most recent texture has space
	if (mTextures.size())
	{
		tex_out = &mTextures.back();
		if (tex_out->findEmpty(glyphSize, cursor_out)) // will this one work?
			return; // yes
	}

	// current textures are full, so make a new one
	mTextures.push_back(FontTexture());
	tex_out = &mTextures.back();
	tex_out->initTexture();

	if (!tex_out->findEmpty(glyphSize, cursor_out))
	{
		LOG(LogError) << "Glyph too big to fit on a new texture (glyph size > " << tex_out->textureSize.x() << ", " << tex_out->textureSize.y() << ")!";
		tex_out = NULL;
	}
}

std::vector<std::string> getFallbackFontPaths()
{
#ifdef WIN32 // Windows

	// get this system's equivalent of "C:\Windows" (might be on a different drive or in a different folder)
	// so we can check the Fonts subdirectory for fallback fonts
	TCHAR winDir[MAX_PATH];
	GetWindowsDirectory(winDir, MAX_PATH);
	const std::string fontDir = std::string(winDir) + "\\Fonts\\";

	const std::vector<const char*> fontNames = {
		"meiryo.ttc", // japanese
		"simhei.ttf", // chinese
		"arial.ttf" // latin
	};

	std::vector<std::string> fontPaths;
	fontPaths.reserve(fontNames.size());
	for (const auto& it : fontNames)
	{
		const std::string path = fontDir + it;
		if (ResourceManager::getInstance()->fileExists(path))
			fontPaths.push_back(path);
	}

#else // (Linux)

	// TODO
	const std::vector<const char*> fontNames = {
		"/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf", // japanese, chinese, present on Debian
		":/fontawesome_webfont.ttf",
		"/usr/share/fonts/truetype/fontawesome-webfont.ttf",
	};

	std::vector<std::string> fontPaths;
	fontPaths.reserve(fontNames.size());
	for (const auto& it : fontNames)
	{
		if (ResourceManager::getInstance()->fileExists(it))
			fontPaths.push_back(it);
	}

#endif
	fontPaths.shrink_to_fit();
	return fontPaths;
}

FT_Face Font::getFaceForChar(UnicodeChar id) const
{
	static const std::vector<std::string> fallbackFonts = getFallbackFontPaths();

	// look through our current font + fallback fonts to see if any have the glyph we're looking for
	for (unsigned int i = 0; i < fallbackFonts.size() + 1; i++)
	{
		auto fit = mFaceCache.find(i);

		if (fit == mFaceCache.end()) // doesn't exist yet
		{
			// i == 0 -> mPath
			// otherwise, take from fallbackFonts
			const std::string& path = (i == 0 ? mPath : fallbackFonts.at(i - 1));
			ResourceData data = ResourceManager::getInstance()->getFileData(path);
			mFaceCache[i] = std::unique_ptr<FontFace>(new FontFace(std::move(data), mSize));
			fit = mFaceCache.find(i);
		}

		if (FT_Get_Char_Index(fit->second->face, id) != 0)
			return fit->second->face;
	}

	// nothing has a valid glyph - return the "real" face so we get a "missing" character
	return mFaceCache.begin()->second->face;
}

Font::Glyph* Font::getGlyph(UnicodeChar id)
{
	auto it = mGlyphMap.find(id); // is it already loaded?
	if (it != mGlyphMap.end())
		return &it->second;

	// nope, need to make a glyph
	FT_Face face = getFaceForChar(id);
	if (face == nullptr)
	{
		LOG(LogError) << "Could not find appropriate font face for character " << id << " for font " << mPath;
		return NULL;
	}

	const FT_GlyphSlot g = face->glyph;

	if (FT_Load_Char(face, id, FT_LOAD_RENDER))
	{
		LOG(LogError) << "Could not find glyph for character " << id << " for font " << mPath << ", size " << mSize << "!";
		return nullptr;
	}

	const Eigen::Vector2i glyphSize(g->bitmap.width, g->bitmap.rows);

	FontTexture* tex = nullptr;
	Eigen::Vector2i cursor;
	getTextureForNewGlyph(glyphSize, tex, cursor);

	// getTextureForNewGlyph can fail if the glyph is bigger than the max texture size (absurdly large font size)
	if (tex == nullptr)
	{
		LOG(LogError) << "Could not create glyph for character " << id << " for font " << mPath << ", size " << mSize
					  << " (no suitable texture found)!";
		return nullptr;
	}

	// create glyph
	Glyph& glyph = mGlyphMap[id];

	glyph.texture = tex;
	glyph.texPos << cursor.x() / (float)tex->textureSize.x(), cursor.y() / (float)tex->textureSize.y();
	glyph.texSize << glyphSize.x() / (float)tex->textureSize.x(), glyphSize.y() / (float)tex->textureSize.y();

	glyph.advance << (float)g->metrics.horiAdvance / 64.0f, (float)g->metrics.vertAdvance / 64.0f;
	glyph.bearing << (float)g->metrics.horiBearingX / 64.0f, (float)g->metrics.horiBearingY / 64.0f;

	// upload glyph bitmap to texture
	glBindTexture(GL_TEXTURE_2D, tex->textureId);
	glTexSubImage2D(GL_TEXTURE_2D, 0, cursor.x(), cursor.y(), glyphSize.x(), glyphSize.y(), GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap.buffer);
	glBindTexture(GL_TEXTURE_2D, 0);

	// update max glyph height
	if (glyphSize.y() > mMaxGlyphHeight)
		mMaxGlyphHeight = glyphSize.y();

	return &glyph;
}

const Font::Glyph* Font::getGlyph(UnicodeChar id) const
{
	return const_cast<Font&>(*this).getGlyph(id);
}

// completely recreate the texture data for all textures based on mGlyphs information
void Font::rebuildTextures()
{
	// recreate OpenGL textures
	for (auto& it : mTextures)
	{
		it.deinitTexture();
		it.initTexture();
	}

	// reupload the texture data
	for (const auto& it : mGlyphMap)
	{
		const FT_Face face = getFaceForChar(it.first);
		const FT_GlyphSlot glyphSlot = face->glyph;

		// load the glyph bitmap through FT
		FT_Load_Char(face, it.first, FT_LOAD_RENDER);

		const FontTexture* tex = it.second.texture;

		// find the position/size
		const Eigen::Vector2i cursor(it.second.texPos.x() * tex->textureSize.x(), it.second.texPos.y() * tex->textureSize.y());
		const Eigen::Vector2i glyphSize(it.second.texSize.x() * tex->textureSize.x(), it.second.texSize.y() * tex->textureSize.y());

		// upload to texture
		glBindTexture(GL_TEXTURE_2D, tex->textureId);
		glTexSubImage2D(GL_TEXTURE_2D, 0, cursor.x(), cursor.y(), glyphSize.x(), glyphSize.y(), GL_ALPHA, GL_UNSIGNED_BYTE, glyphSlot->bitmap.buffer);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
}

void Font::renderTextCache(TextCache* cache)
{
	if (cache == NULL)
	{
		LOG(LogError) << "Attempted to draw NULL TextCache!";
		return;
	}

	for (auto it = cache->vertexLists.begin(); it != cache->vertexLists.end(); it++)
	{
		assert(*it->textureIdPtr != 0);

		glBindTexture(GL_TEXTURE_2D, *it->textureIdPtr);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		glVertexPointer(2, GL_FLOAT, sizeof(TextCache::Vertex), it->verts[0].pos.data());
		glTexCoordPointer(2, GL_FLOAT, sizeof(TextCache::Vertex), it->verts[0].tex.data());
		glColorPointer(4, GL_UNSIGNED_BYTE, 0, it->colors.data());

		glDrawArrays(GL_TRIANGLES, 0, it->verts.size());

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);

		glDisable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
	}
}

Eigen::Vector2f Font::sizeText(const std::string& text, float lineSpacing)
{
	float lineWidth = 0.0f;
	float highestWidth = 0.0f;

	const float lineHeight = getHeight(lineSpacing);

	float y = lineHeight;

	size_t i = 0;
	while (i < text.length())
	{
		UnicodeChar character = readUnicodeChar(text, i); // advances i

		if (character == (UnicodeChar)'\n')
		{
			if (lineWidth > highestWidth)
				highestWidth = lineWidth;

			lineWidth = 0.0f;
			y += lineHeight;
		}

		Glyph* glyph = getGlyph(character);
		if (glyph != nullptr)
			lineWidth += glyph->advance.x();
	}

	if (lineWidth > highestWidth)
		highestWidth = lineWidth;

	return Eigen::Vector2f(highestWidth, y);
}

float Font::getHeight(float lineSpacing) const
{
	return mMaxGlyphHeight * lineSpacing;
}

float Font::getLetterHeight()
{
	const Glyph* glyph = getGlyph(static_cast<UnicodeChar>('S'));
	assert(glyph != nullptr);
	return glyph->texSize.y() * glyph->texture->textureSize.y();
}

// the worst algorithm ever written
// breaks up a normal string with newlines to make it fit xLen
std::string Font::wrapText(std::string text, float xLen)
{
	std::string out;
	std::string line;

	while (text.length() > 0) // while there's text or we still have text to render
	{
		size_t space = text.find_first_of(" \t\n");
		if (space == std::string::npos)
			space = text.length() - 1;

		const std::string word = text.substr(0, space + 1);
		text.erase(0, space + 1);

		const std::string temp = line + word;

		const Eigen::Vector2f textSize = sizeText(temp);

		// if the word will fit on the line, add it to our line, and continue
		if (textSize.x() <= xLen)
		{
			line = temp;
			continue;
		}
		else // the next word won't fit, so break here
		{
			out += line + '\n';
			line = word;
		}
	}

	out += line; // whatever is left should fit

	return out;
}

Eigen::Vector2f Font::sizeWrappedText(const std::string& text, float xLen, float lineSpacing)
{
	return sizeText(wrapText(text, xLen), lineSpacing);
}

Eigen::Vector2f Font::getWrappedTextCursorOffset(const std::string& text, float xLen, size_t stop, float lineSpacing)
{
	const std::string wrappedText = wrapText(text, xLen);

	float lineWidth = 0.0f;
	float y = 0.0f;

	size_t wrapCursor = 0;
	size_t cursor = 0;
	while (cursor < stop)
	{
		UnicodeChar wrappedCharacter = readUnicodeChar(wrappedText, wrapCursor);
		UnicodeChar character = readUnicodeChar(text, cursor);

		if (wrappedCharacter == (UnicodeChar)'\n' && character != (UnicodeChar)'\n')
		{
			// this is where the wordwrap inserted a newline
			// reset lineWidth and increment y, but don't consume a cursor character
			lineWidth = 0.0f;
			y += getHeight(lineSpacing);

			cursor = getPrevCursor(text, cursor); // unconsume
			continue;
		}

		if (character == (UnicodeChar)'\n')
		{
			lineWidth = 0.0f;
			y += getHeight(lineSpacing);
			continue;
		}

		Glyph* glyph = getGlyph(character);
		if (glyph != nullptr)
			lineWidth += glyph->advance.x();
	}

	return Eigen::Vector2f(lineWidth, y);
}

//=============================================================================================================
// TextCache
//=============================================================================================================

float Font::getNewlineStartOffset(const std::string& text, const unsigned int& charStart, const float& xLen, const Alignment& alignment)
{
	switch (alignment)
	{
	case ALIGN_LEFT:
		return 0;
	case ALIGN_CENTER:
	{
		const size_t endChar = text.find('\n', charStart);
		return (xLen - sizeText(text.substr(charStart, endChar != std::string::npos ? endChar - charStart : endChar)).x()) / 2.0f;
	}
	case ALIGN_RIGHT:
	{
		const size_t endChar = text.find('\n', charStart);
		return xLen - (sizeText(text.substr(charStart, endChar != std::string::npos ? endChar - charStart : endChar)).x());
	}
	default:
		return 0;
	}
}

TextCache* Font::buildTextCache(
	const std::string& text, Eigen::Vector2f offset, unsigned int color, float xLen, Alignment alignment, float lineSpacing)
{
	float x = offset[0] + (xLen != 0 ? getNewlineStartOffset(text, 0, xLen, alignment) : 0);

	const float yTop = getGlyph((UnicodeChar)'S')->bearing.y();
	const float yBot = getHeight(lineSpacing);
	float y = offset[1] + (yBot + yTop) / 2.0f;

	// vertices by texture
	std::map<FontTexture*, std::vector<TextCache::Vertex>> vertMap;

	size_t cursor = 0;
	

	while (cursor < text.length())
	{
		const UnicodeChar character = readUnicodeChar(text, cursor); // also advances cursor
		if (character == 0) // invalid character?
			continue;

		if (character == static_cast<UnicodeChar>('\n'))
		{
			y += getHeight(lineSpacing);
			x = offset[0] + (xLen != 0 ? getNewlineStartOffset(text, cursor /* cursor is already advanced */, xLen, alignment) : 0);
			continue;
		}

		const Glyph* glyph = getGlyph(character);
		if (glyph == nullptr)
			continue;

		std::vector<TextCache::Vertex>& verts = vertMap[glyph->texture];
		size_t oldVertSize = verts.size();
		verts.resize(oldVertSize + 6);
		TextCache::Vertex* tri = verts.data() + oldVertSize;

		const float glyphStartX = x + glyph->bearing.x();

		const Eigen::Vector2i& textureSize = glyph->texture->textureSize;

		// triangle 1
		// round to fix some weird "cut off" text bugs
		tri[0].pos << font_round(glyphStartX), font_round(y + (glyph->texSize.y() * textureSize.y() - glyph->bearing.y()));
		tri[1].pos << font_round(glyphStartX + glyph->texSize.x() * textureSize.x()), font_round(y - glyph->bearing.y());
		tri[2].pos << tri[0].pos.x(), tri[1].pos.y();

		// tri[0].tex << 0, 0;
		// tri[0].tex << 1, 1;
		// tri[0].tex << 0, 1;

		tri[0].tex << glyph->texPos.x(), glyph->texPos.y() + glyph->texSize.y();
		tri[1].tex << glyph->texPos.x() + glyph->texSize.x(), glyph->texPos.y();
		tri[2].tex << tri[0].tex.x(), tri[1].tex.y();

		// triangle 2
		tri[3].pos = tri[0].pos;
		tri[4].pos = tri[1].pos;
		tri[5].pos << tri[1].pos.x(), tri[0].pos.y();

		tri[3].tex = tri[0].tex;
		tri[4].tex = tri[1].tex;
		tri[5].tex << tri[1].tex.x(), tri[0].tex.y();

		// advance
		x += glyph->advance.x();
	}

	// TextCache::CacheMetrics metrics = { sizeText(text, lineSpacing) };

	TextCache* cache = new TextCache();
	cache->vertexLists.resize(vertMap.size());
	cache->metrics = {sizeText(text, lineSpacing)};

	for (const auto& it : vertMap)
	{
		TextCache::VertexList& vertList = cache->vertexLists.at(0ull);

		vertList.textureIdPtr = &it.first->textureId;
		vertList.verts = it.second;

		vertList.colors.resize(4 * it.second.size());
		Renderer::buildGLColorArray(vertList.colors.data(), color, it.second.size());
	}

	mFaceCache.clear();

	return cache;
}

TextCache* Font::buildTextCache(const std::string& text, float offsetX, float offsetY, unsigned int color)
{
	return buildTextCache(text, Eigen::Vector2f(offsetX, offsetY), color, 0.0f);
}

void TextCache::setColor(unsigned int color)
{
	for (auto& it : vertexLists)
		Renderer::buildGLColorArray(it.colors.data(), color, it.verts.size());
}

std::shared_ptr<Font> Font::getFromTheme(const ThemeData::ThemeElement* elem, unsigned int properties, const std::shared_ptr<Font>& orig)
{
	if (!(properties & ThemeFlags::FONT_PATH) && !(properties & ThemeFlags::FONT_SIZE))
		return orig;

	std::shared_ptr<Font> font;
	int size = (orig ? orig->mSize : FONT_SIZE_MEDIUM);
	std::string path = (orig ? orig->mPath : getDefaultPath());

	const float sh = static_cast<float>(Renderer::getScreenHeight());
	if (properties & ThemeFlags::FONT_SIZE && elem->has("fontSize"))
		size = static_cast<int>(sh * elem->get<float>("fontSize"));
	if (properties & ThemeFlags::FONT_PATH && elem->has("fontPath"))
		path = elem->get<std::string>("fontPath");

	return get(size, path);
}
