#include "SVGResource.h"
#include "nanosvg/nanosvg.h"
#include "nanosvg/nanosvgrast.h"
#include "ImageIO.h"
#include "Log.h"
//#include "Util.h"

#define DPI 96

SVGResource::SVGResource(const std::string& path, bool tile)
	: TextureResource(path, tile)
	, mSVGImage(NULL)
    , mLastWidth{}
    , mLastHeight{}
{
}

SVGResource::~SVGResource()
{
	deinitSVG();
}

void SVGResource::unload(std::shared_ptr<ResourceManager>& rm)
{
	deinitSVG();
	TextureResource::unload(rm);
}

void SVGResource::initFromMemory(const char* file, size_t length)
{
	deinit();
	deinitSVG();

	// nsvgParse excepts a modifiable, null-terminated string
	char* copy = (char*)malloc(length + 1);
	assert(copy != NULL);
	memcpy(copy, file, length);
	copy[length] = '\0';

	mSVGImage = nsvgParse(copy, "px", DPI);
	free(copy);

	if(mSVGImage == nullptr)
	{
		LOG(LogError) << "Error parsing SVG image.";
		return;
	}

	if (mLastWidth && mLastHeight)
		rasterizeAt(mLastWidth, mLastHeight);
	else
		rasterizeAt(static_cast<size_t>(round(mSVGImage->width)), static_cast<size_t>(round(mSVGImage->height)));
}

void SVGResource::rasterizeAt(size_t width, size_t height)
{
	if (mSVGImage == nullptr || (width == 0 && height == 0))
		return;

	if (width == 0)
		width = (size_t)round((height / mSVGImage->height) * mSVGImage->width); // auto scale width to keep aspect
	else if (height == 0)
		height = (size_t)round((width / mSVGImage->width) * mSVGImage->height); // auto scale height to keep aspect

	if (width != (size_t)round(mSVGImage->width) && height != (size_t)round(mSVGImage->height))
	{
		mLastWidth = width;
		mLastHeight = height;
	}

	unsigned char* imagePx = static_cast<unsigned char*>(malloc(width * height * 4));
	assert(imagePx != nullptr);

	NSVGrasterizer* rast = nsvgCreateRasterizer();
	nsvgRasterize(rast, mSVGImage, 0, 0, height / mSVGImage->height, imagePx, width, height, width * 4);
	nsvgDeleteRasterizer(rast);

	ImageIO::flipPixelsVert(imagePx, width, height);

	initFromPixels(imagePx, width, height);
	free(imagePx);
}

Eigen::Vector2f SVGResource::getSourceImageSize() const
{
	return (mSVGImage != nullptr) ? Eigen::Vector2f(mSVGImage->width, mSVGImage->height) : Eigen::Vector2f::Zero();
}

void SVGResource::deinitSVG()
{
	if(mSVGImage != nullptr)
		nsvgDelete(mSVGImage);

	mSVGImage = NULL;
}
