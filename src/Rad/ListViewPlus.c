#include "ListViewPlus.h"

#include "WindowsPlus.h"

//#include <commctrl.h>
//#include <Windowsx.h>

BOOL ListView_EnsureSubItemVisible(HWND hWnd, _In_ int nItem, _In_ int nCol, _In_ BOOL bPartialOK)
{
    if (!ListView_EnsureVisible(hWnd, nItem, bPartialOK))
        return FALSE;

    RECT rect = { 0 };
    if (!ListView_GetSubItemRect(hWnd, nItem, nCol, LVIR_BOUNDS, &rect))
        return FALSE;

    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    if (rect.left < rcClient.left || Width(rect) > Width(rcClient))
        return ListView_Scroll(hWnd, rect.left - rcClient.left, 0);
    else if (rect.right > rcClient.right)
        return ListView_Scroll(hWnd, rect.right - rcClient.right, 0);
    else
        return TRUE;
}
