#pragma once

#include "Rad/Log.h"

template <class T>
T QueryRegDWORDValue(CRegKey& key, LPCTSTR name, T def)
{
    DWORD v;
    if (key && key.QueryDWORDValue(name, v) == ERROR_SUCCESS)
        return v;
    else
        return def;
}

inline CString RegQueryStringValue(CRegKey& key, LPCTSTR name, const CString& def)
{
    CString s;
    ULONG sz = 1024;
    if (key && key.QueryStringValue(name, s.GetBufferSetLength(sz), &sz) == ERROR_SUCCESS)
        return s.ReleaseBuffer(), s;
    else
        return def;
}

inline CString ExpandEnvironmentStrings(LPCTSTR s)
{
    CString r;
    CHECK(ExpandEnvironmentStrings(s, r.GetBufferSetLength(1024), 1024));
    r.ReleaseBuffer();
    return r;
}

inline CString PathCanonicalize(LPCTSTR s)
{
    CString r;
    CHECK(PathCanonicalize(r.GetBufferSetLength(1024), s));
    r.ReleaseBuffer();
    return r;
}

inline void SetMenuItemBitmap(_In_ HMENU hMenu, _In_ UINT item, _In_ BOOL fByPosition, _In_ HBITMAP hBitmap)
{
    MENUITEMINFO minfo;
    minfo.cbSize = sizeof(minfo);
    minfo.fMask = MIIM_BITMAP;
    minfo.hbmpItem = hBitmap;
    SetMenuItemInfo(hMenu, item, fByPosition, &minfo);
}

inline void SetMenuItemData(_In_ HMENU hMenu, _In_ UINT item, _In_ BOOL fByPosition, _In_ ULONG_PTR dwItemData)
{
    MENUITEMINFO minfo;
    minfo.cbSize = sizeof(minfo);
    minfo.fMask = MIIM_DATA;
    minfo.dwItemData = dwItemData;
    SetMenuItemInfo(hMenu, item, fByPosition, &minfo);
}
