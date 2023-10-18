#pragma once

#include <Windows.h>
#include <combaseapi.h>

interface IShellItem;

struct JumpListData;
JumpListData* FillJumpListMenu(HMENU hMenu, IShellItem* pShellItem); // JumpListMenu.cpp
void DoJumpListMenu(HWND hWnd, JumpListData* pData, int id); // JumpListMenu.cpp
int JumpListMenuGetIcon(JumpListData* pData, int id); // JumpListMenu.cpp
