#pragma once

#include <Windows.h>
#include <combaseapi.h>

interface IShellItem;

struct JumpListData;
JumpListData* FillJumpListMenu(HMENU hMenu, IShellItem* pShellItem);
void DoJumpListMenu(HWND hWnd, JumpListData* pData, int id);
int JumpListMenuGetSystemIcon(JumpListData* pData, int id);
HICON JumpListMenuGetIcon(JumpListData* pData, int id);
