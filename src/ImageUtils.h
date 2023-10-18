#pragma once
#include <Windows.h>

// Loads an image file into a bitmap and optionally resizes it
HBITMAP LoadImageFile(LPCWSTR path, const SIZE* pSize, bool bUseAlpha, bool bPremultiply);
