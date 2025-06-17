#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <combaseapi.h>

interface IShellItem;

void FillJumpListMenu(HMENU hMenu, HIMAGELIST hImageListMenu, IShellItem* pShellItem);
void DoJumpListMenu(HMENU hMenu, HWND hWnd, int id);
