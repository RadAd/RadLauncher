#pragma once
#include "Rad/MessageHandler.h"
#include "Rad/MemoryPlus.h"
#include "Rad/Log.h"
#include "Rad/Windowxx.h"
#include "JumpListMenu.h"
#include <ShlObj.h>

class JumpListMenuHandler : public MessageChain
{
public:
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
        ::DoJumpListMenu(hMenu.get(), hWnd, id);
        return id > 0;
    }

protected:
    BOOL OnCreate(const LPCREATESTRUCT lpCreateStruct)
    {
        HIMAGELIST hImageListLg, hImageListSm;
        CHECK(Shell_GetImageLists(&hImageListLg, &hImageListSm));

        m_hImageListMenu = hImageListSm;

        return TRUE;
    }

    void OnMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem)
    {
        if (lpMeasureItem->CtlID == 0 || lpMeasureItem->CtlType == ODT_MENU)
        {
            OwnerDrawnMenuItem* od = reinterpret_cast<OwnerDrawnMenuItem*>(lpMeasureItem->itemData);
            if (od)
                od->OnMeasureItem(lpMeasureItem);
        }
        else
            SetHandled(false);
    }

    void OnDrawItem(const DRAWITEMSTRUCT* lpDrawItem)
    {
        if (lpDrawItem->CtlID == 0 || lpDrawItem->CtlType == ODT_MENU)
        {
            OwnerDrawnMenuItem* od = reinterpret_cast<OwnerDrawnMenuItem*>(lpDrawItem->itemData);
            if (od)
                od->OnDrawItem(lpDrawItem);
        }
        else
            SetHandled(false);
    }

    virtual LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
    {
        LRESULT ret = 0;
        switch (uMsg)
        {
            HANDLE_MSG(WM_CREATE, OnCreate);
            HANDLE_MSG(WM_MEASUREITEM, OnMeasureItem);
            HANDLE_MSG(WM_DRAWITEM, OnDrawItem);
        }

#if 0
        if (!IsHandled())
            ret = MessageChain::HandleMessage(uMsg, wParam, lParam);
#endif

        return ret;
    }

private:
    HIMAGELIST m_hImageListMenu = NULL;
};
