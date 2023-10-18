#define STRICT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "ImageUtils.h"

#include <atlbase.h>
#include <wincodec.h>
#include <shlwapi.h>

#include "Rad/Log.h"
#include "Rad/MemoryPlus.h"

// Loads an image file into a bitmap and optionally resizes it
HBITMAP LoadImageFile(LPCWSTR path, const SIZE* pSize, bool bUseAlpha, bool bPremultiply)
{
    auto srcBmp = MakeUniqueHandle(HBITMAP(NULL), DeleteObject);
    if (_wcsicmp(PathFindExtension(path), L".bmp") == 0)
    {
        srcBmp.reset((HBITMAP) LoadImage(NULL, path, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_LOADFROMFILE));
    }
    if (srcBmp && !pSize)   // TODO Check size of srcBmp matches pSize
        return srcBmp.release();

    CComPtr<IWICImagingFactory> pFactory;
    CHECK_HR(pFactory.CoCreateInstance(CLSID_WICImagingFactory));

    CComPtr<IWICBitmapSource> pBitmap;
    if (srcBmp)
    {
        CComPtr<IWICBitmap> pBitmap2;
        CHECK_HR(pFactory->CreateBitmapFromHBITMAP(srcBmp.get(), NULL, bUseAlpha ? WICBitmapUseAlpha : WICBitmapIgnoreAlpha, &pBitmap2));

        pBitmap = pBitmap2;
        srcBmp.reset();
    }
    else
    {
        CComPtr<IWICBitmapDecoder> pDecoder;
        CHECK_HR(pFactory->CreateDecoderFromFilename(path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder));

        CComPtr<IWICBitmapFrameDecode> pFrame;
        CHECK_HR(pDecoder->GetFrame(0, &pFrame));

        pBitmap = pFrame;
    }

    {
        CComPtr<IWICFormatConverter> pConverter;
        CHECK_HR(pFactory->CreateFormatConverter(&pConverter));
        pConverter->Initialize(pBitmap, bPremultiply ? GUID_WICPixelFormat32bppPBGRA : GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0, WICBitmapPaletteTypeMedianCut);
        pBitmap = pConverter;
    }

    UINT width = 0, height = 0;
    pBitmap->GetSize(&width, &height);

    const LONG w = pSize ? pSize->cx : width;
    const LONG h = pSize ? pSize->cy : height;

    if (w != width || h != height)
    {
        CComPtr<IWICBitmapScaler> pScaler;
        CHECK_HR(pFactory->CreateBitmapScaler(&pScaler));
        pScaler->Initialize(pBitmap, w, h, WICBitmapInterpolationModeFant);
        pBitmap = pScaler;
    }

#if 1
    BITMAPINFO bi = { 0 };
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;

    HDC hdc = CreateCompatibleDC(NULL);
    BYTE* pBits = nullptr;
    HBITMAP bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void**) &pBits, NULL, 0);
    DeleteDC(hdc);

    const int stride = w * 4;
    const int frameSize = h * stride;
    pBitmap->CopyPixels(nullptr, stride, frameSize, pBits);
#else
    const int stride = w * 4;
    const int frameSize = h * stride;
    BYTE* pBits = new BYTE[frameSize];
    pBitmap->CopyPixels(NULL, stride, frameSize, pBits);
    HBITMAP bmp = CreateBitmap(w, h, 1, 32, pBits);
#endif

    return bmp;
}
