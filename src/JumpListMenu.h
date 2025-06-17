#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <combaseapi.h>

interface IShellItem;

class OwnerDrawnMenuItem
{
public:
    virtual ~OwnerDrawnMenuItem() {}

    virtual void OnMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem) = 0;
    virtual void OnDrawItem(const DRAWITEMSTRUCT* lpDrawItem) = 0;
};

void FillJumpListMenu(HMENU hMenu, HIMAGELIST hImageListMenu, IShellItem* pShellItem);
void DoJumpListMenu(HMENU hMenu, HWND hWnd, int id);
