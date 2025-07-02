#pragma once
#include "OwnerDrawnMenuHandler.h"
#include "Rad/MemoryPlus.h"
#include "Rad/Log.h"
#include "JumpListMenu.h"
#include <ShlObj.h>

class JumpListMenuHandler : public OwnerDrawnMenuHandler
{
public:
    JumpListMenuHandler()
    {
        HIMAGELIST hImageListLg, hImageListSm;
        CHECK(Shell_GetImageLists(&hImageListLg, &hImageListSm));

        m_hImageListMenu = hImageListSm;
    }

    bool DoJumpListMenu(HWND hWnd, IShellItem* pShellItem, const POINT pt)
    {
        const auto hMenu = MakeUniqueHandle(CreatePopupMenu(), DestroyMenu);

        FillJumpListMenu(hMenu.get(), m_hImageListMenu, pShellItem);

        MENUINFO mnfo;
        mnfo.cbSize = sizeof(mnfo);
        mnfo.fMask = MIM_STYLE;
        mnfo.dwStyle = MNS_CHECKORBMP | MNS_AUTODISMISS;
        CHECK_LE(SetMenuInfo(hMenu.get(), &mnfo));

        int id = TrackPopupMenu(hMenu.get(), TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hWnd, nullptr);
        if (id <= 0)
            return false;
        ::DoJumpListMenu(hMenu.get(), hWnd, id);
        return true;
    }

private:
    HIMAGELIST m_hImageListMenu = NULL;
};
