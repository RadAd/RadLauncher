#pragma once
#include "Rad/MessageHandler.h"
#include "Rad/Windowxx.h"

class OwnerDrawnMenuItem
{
public:
    virtual ~OwnerDrawnMenuItem() {}

    virtual void OnMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem) = 0;
    virtual void OnDrawItem(const DRAWITEMSTRUCT* lpDrawItem) = 0;
};

class OwnerDrawnMenuHandler : public MessageChain
{
protected:
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
            HANDLE_MSG(WM_MEASUREITEM, OnMeasureItem);
            HANDLE_MSG(WM_DRAWITEM, OnDrawItem);
        }

#if 0
        if (!IsHandled())
            ret = MessageChain::HandleMessage(uMsg, wParam, lParam);
#endif

        return ret;
    }
};
