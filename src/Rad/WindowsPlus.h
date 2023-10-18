#pragma once
#include <Windows.h>

#include <strsafe.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C"
{
#endif

HWND FindOwnedWindow(HWND hOwner, LPCTSTR lpClassName, LPCTSTR lpWindowName);

// sz : ICON_BIG or ICON_SMALL
inline HICON LoadIconImage(HINSTANCE hInstance, LPCTSTR resource, DWORD sz)
{
    const int CX_ICON = sz == ICON_SMALL ? GetSystemMetrics(SM_CXSMICON) : GetSystemMetrics(SM_CXICON);
    const int CY_ICON = sz == ICON_SMALL ? GetSystemMetrics(SM_CYSMICON) : GetSystemMetrics(SM_CYICON);
    return (HICON)LoadImage(hInstance, resource, IMAGE_ICON, CX_ICON, CY_ICON, LR_SHARED);
}

inline HMENU LoadPopupMenu(HINSTANCE hInstance, DWORD id)
{
    const HMENU hMenu = LoadMenu(hInstance, MAKEINTRESOURCE(id));
    const HMENU hPopupMenu = GetSubMenu(hMenu, 0);
    RemoveMenu(hMenu, 0, MF_BYPOSITION);
    DestroyMenu(hMenu);
    return hPopupMenu;
}

inline RECT Rect(const POINT pt, const SIZE sz)
{
    const RECT rc = { pt.x, pt.y, pt.x + sz.cx, pt.y + sz.cy };
    return rc;
}

inline LONG Width(const RECT rc)
{
    return rc.right - rc.left;
}

inline LONG Height(const RECT rc)
{
    return rc.bottom - rc.top;
}

inline BOOL SetDlgItemHexU8(HWND hDlg, int nIDDlgItem, UINT8 value)
{
    TCHAR msg[100];
    StringCchPrintf(msg, ARRAYSIZE(msg), TEXT("%02X"), value);
    return SetDlgItemText(hDlg, nIDDlgItem, msg);
}

inline UINT8 GetDlgItemHexU8(HWND hDlg, int nIDDlgItem)
{
    TCHAR msg[100];
    GetDlgItemText(hDlg, nIDDlgItem, msg, ARRAYSIZE(msg));

    return (UINT8) wcstoul(msg, NULL, 16);
}

inline BOOL SetDlgItemHexU16(HWND hDlg, int nIDDlgItem, UINT16 value)
{
    TCHAR msg[100];
    StringCchPrintf(msg, ARRAYSIZE(msg), TEXT("%04X"), value);
    return SetDlgItemText(hDlg, nIDDlgItem, msg);
}

inline UINT16 GetDlgItemHexU16(HWND hDlg, int nIDDlgItem)
{
    TCHAR msg[100];
    GetDlgItemText(hDlg, nIDDlgItem, msg, ARRAYSIZE(msg));

    return (UINT16) wcstoul(msg, NULL, 16);
}

#if 0
typedef int GetValue(int i, void* context);
inline int BinarySearch(GetValue* gv, void* context, const int n, const int key)
{
    int low = 0;
    int high = n - 1;
    while (low <= high)
    {
        int mid = (low + high) / 2;
        const int value = gv(mid, context);
        if (value < key)
            low = mid + 1;
        else if (value == key)
            return mid;
        else
            high = mid - 1;
    }
    return -1;
}
#endif

#ifdef __cplusplus
}
#endif
