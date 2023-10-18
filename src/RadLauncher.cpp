#include "Rad/Window.h"
#include "Rad/Windowxx.h"
#include "Rad/Log.h"
#include "Rad/ListViewPlus.h"
#include "Rad/MemoryPlus.h"
#include "Rad/Format.h"
#include "Rad/WindowsPlus.h"

#include <ShlObj.h>

#include "ShellUtils.h"
#include "WindowsUtils.h"
#include "JumpListMenu.h"

#define APPNAME TEXT("RadLauncher")
#define APPNAMEA "RadLauncher"
#define APPNAMEW L"RadLauncher"

//#include <tchar.h>
//#include <strsafe.h>
//#include "resource.h"

#define ID_LIST                         101

#define HK_OPEN                 1

class RootWindow : public Window
{
    friend WindowManager<RootWindow>;
public:
    static ATOM Register() { return WindowManager<RootWindow>::Register(); }
    static RootWindow* Create() { return WindowManager<RootWindow>::Create(); }

protected:
    static void GetCreateWindow(CREATESTRUCT& cs);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnDestroy();
    void OnSize(UINT state, int cx, int cy);
    void OnSetFocus(HWND hwndOldFocus);
    LRESULT OnNotify(const DWORD dwID, const LPNMHDR pNmHdr);
    void OnContextMenu(HWND hWndContext, UINT xPos, UINT yPos);
    void OnHotKey(int idHotKey, UINT fuModifiers, UINT vk);
    void OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu);
    void OnMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem);
    void OnDrawItem(const DRAWITEMSTRUCT* lpDrawItem);

    static LPCTSTR ClassName() { return APPNAME; }

    void Refresh();

    CComPtr<IShellFolder> m_pFolder;
    //LPITEMIDLIST m_pIdList;
    HWND m_hWndChild = NULL;
    HIMAGELIST m_hImageListMenu = NULL;
    JumpListData* m_pjld = nullptr;
};

void RootWindow::GetCreateWindow(CREATESTRUCT& cs)
{
    Window::GetCreateWindow(cs);
    cs.lpszName = APPNAME;
    cs.style = WS_OVERLAPPEDWINDOW;
    cs.dwExStyle |= WS_EX_TOOLWINDOW;

    CRegKey reg;
    reg.Open(HKEY_CURRENT_USER, TEXT(R"(Software\RadSoft\)") APPNAME);

    if (reg)
    {
        cs.x = QueryRegDWORDValue(reg, TEXT("x"), cs.x);
        cs.y = QueryRegDWORDValue(reg, TEXT("y"), cs.y);
        cs.cx = QueryRegDWORDValue(reg, TEXT("cx"), cs.cx);
        cs.cy = QueryRegDWORDValue(reg, TEXT("cy"), cs.cy);
    }
}

BOOL RootWindow::OnCreate(const LPCREATESTRUCT lpCreateStruct)
{
    HIMAGELIST hImageListLg, hImageListSm;
    CHECK(Shell_GetImageLists(&hImageListLg, &hImageListSm));

    m_hImageListMenu = hImageListSm;

    CRegKey reg;
    reg.Open(HKEY_CURRENT_USER, TEXT(R"(Software\RadSoft\)") APPNAME);

    m_hWndChild = ListView_Create(*this, RECT(), WS_CHILD | WS_VISIBLE | LVS_ALIGNTOP | LVS_ICON | LVS_SINGLESEL | LVS_SHAREIMAGELISTS | LVS_AUTOARRANGE, LVS_EX_FLATSB, ID_LIST);
    _ASSERT(m_hWndChild);
    CHECK(ListView_EnableGroupView(m_hWndChild, TRUE) >= 0);
    ListView_SetImageList(m_hWndChild, hImageListLg, LVSIL_NORMAL);
    ListView_SetImageList(m_hWndChild, hImageListSm, LVSIL_SMALL);
    ListView_SetGroupHeaderImageList(m_hWndChild, hImageListSm);
    ListView_SetIconSpacing(m_hWndChild, 80, 0);
    ListView_SetExtendedListViewStyle(m_hWndChild, LVS_EX_TWOCLICKACTIVATE);

    ListView_SetBkColor(m_hWndChild, QueryRegDWORDValue(reg, TEXT("bkcolor"), RGB(0, 0, 0)));
    ListView_SetTextColor(m_hWndChild, QueryRegDWORDValue(reg, TEXT("textcolor"), RGB(250, 250, 250)));
    ListView_SetTextBkColor(m_hWndChild, QueryRegDWORDValue(reg, TEXT("textbkcolor"), RGB(61, 61, 61)));
    //ListView_SetOutlineColor(m_hWndChild, QueryRegDWORDValue(reg, TEXT("outlinekcolor"), RGB(255, 0, 255)));

    //const CString folderspec = IsDebuggerPresent() ? LR"(%__APPDIR__%..\..\Menu)" : RegQueryStringValue(reg, TEXT("folder"), TEXT(R"(%LOCALAPPDATA%\)") APPNAME TEXT(R"(\Menu)"));
    const CString folderspec = RegQueryStringValue(reg, TEXT("folder"), TEXT(R"(%LOCALAPPDATA%\)") APPNAME TEXT(R"(\Menu)"));
    const CString folder = PathCanonicalize(ExpandEnvironmentStrings(folderspec));

    CComPtr<IShellFolder> pDesktopFolder;
    CHECK_HR(SHGetDesktopFolder(&pDesktopFolder));
    CComHeapPtr<ITEMIDLIST> pIdList;
    //CHECK_HR(SHGetKnownFolderIDList(FOLDERID_StartMenu, KF_FLAG_DEFAULT, NULL, &pIdList));
    //CHECK_HR(SHGetKnownFolderIDList(FOLDERID_Programs, KF_FLAG_DEFAULT, NULL, &pIdList));
    //CHECK_HR(SHGetKnownFolderIDList(FOLDERID_CommonPrograms, KF_FLAG_DEFAULT, NULL, &pIdList));
    if (FAILED(SHParseDisplayName(folder, nullptr, &pIdList, 0, nullptr)))
        MessageBox(*this, Format(TEXT("Unable to open folder \"%s\""), LPCWSTR(folder)).c_str(), APPNAME, MB_ICONERROR | MB_OK);
    else
        CHECK_HR(pDesktopFolder->BindToObject(pIdList, nullptr, IID_PPV_ARGS(&m_pFolder)));

    Refresh();

    RegisterHotKey(*this, HK_OPEN, MOD_CONTROL | MOD_ALT, 'L');
    //::SendMessage(*this, WM_SETHOTKEY, MAKEWORD('L', HOTKEYF_CONTROL | HOTKEYF_ALT), 0);

    return TRUE;
}

void RootWindow::OnDestroy()
{
    CRegKey reg;
    reg.Create(HKEY_CURRENT_USER, TEXT(R"(Software\RadSoft\)") APPNAME);

    if (reg)
    {
        RECT rc = {};
        GetWindowRect(*this, &rc);

        reg.SetDWORDValue(TEXT("x"), rc.left);
        reg.SetDWORDValue(TEXT("y"), rc.top);
        reg.SetDWORDValue(TEXT("cx"), Width(rc));
        reg.SetDWORDValue(TEXT("cy"), Height(rc));
    }

    PostQuitMessage(0);
}

void RootWindow::OnSize(const UINT state, const int cx, const int cy)
{
    if (m_hWndChild)
    {
        SetWindowPos(m_hWndChild, NULL, 0, 0,
            cx, cy,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void RootWindow::OnSetFocus(const HWND hwndOldFocus)
{
    if (m_hWndChild)
        SetFocus(m_hWndChild);
}

LRESULT RootWindow::OnNotify(const DWORD dwID, const LPNMHDR pNmHdr)
{
    switch (dwID)
    {
    case ID_LIST:
    {
        switch (pNmHdr->code)
        {
        case LVN_KEYDOWN:
        {
            LPNMLVKEYDOWN pnkd = (LPNMLVKEYDOWN) pNmHdr;
            switch (pnkd->wVKey)
            {
            case VK_F10:
                if (m_pFolder)
                    OpenInExplorer(m_pFolder);
                break;

            case VK_F5:
                Refresh();
                break;

            case VK_ESCAPE:
                ShowWindow(*this, SW_HIDE);
                break;
            }
            break;
        }

        case LVN_DELETEITEM:
        {
            LPNMLISTVIEW pnlv = (LPNMLISTVIEW) pNmHdr;
            IShellItem* pItem = (IShellItem*) pnlv->lParam;
            if (pItem != nullptr)
                pItem->Release();
            break;
        }

        case LVN_ITEMACTIVATE:
        {
            LPNMITEMACTIVATE pnia = (LPNMITEMACTIVATE) pNmHdr;
            //LPCITEMIDLIST pIdList = (LPITEMIDLIST) pnia->lParam;
            IShellItem* pItem = (IShellItem*) ListView_GetItemParam(pnia->hdr.hwndFrom, pnia->iItem);
            if (pItem != nullptr)
            {
                CComPtr<IContextMenu> pContextMenu;
                CHECK_HR(pItem->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&pContextMenu)));

                if (pContextMenu)
                {
                    OpenDefaultItem(*this, pContextMenu);
                    ShowWindow(*this, SW_HIDE);
                }
            }
            break;
        }
        }
        break;
    }
    }
    return 0;
}

void RootWindow::OnContextMenu(HWND hWndContext, UINT xPos, UINT yPos)
{
    if (hWndContext == m_hWndChild)
    {
        if (ListView_GetSelectedCount(m_hWndChild) == 1)
        {
            LVITEMINDEX item = { -1 };
            ListView_GetNextItemIndex(m_hWndChild, &item, LVNI_SELECTED);

            POINT pt = { LONG(xPos), LONG(yPos) };
            if (pt.x == UINT_MAX && pt.y == UINT_MAX)
            {
                RECT rc = {};
                ListView_GetItemRect(m_hWndChild, item.iItem, &rc, LVIR_SELECTBOUNDS);
                pt.x = rc.right;
                pt.y = rc.top;
                ClientToScreen(m_hWndChild, &pt);
            }

            IShellItem* pChildShellItem = (IShellItem*) ListView_GetItemParam(m_hWndChild, item.iItem);
            if (pChildShellItem != nullptr)
            {
                const auto hMenu = MakeUniqueHandle(CreatePopupMenu(), DestroyMenu);

                MENUINFO mnfo;
                mnfo.cbSize = sizeof(mnfo);
                mnfo.fMask = MIM_STYLE;
                mnfo.dwStyle = MNS_CHECKORBMP | MNS_AUTODISMISS;
                SetMenuInfo(hMenu.get(), &mnfo);

                JumpListData* pjld = FillJumpListMenu(hMenu.get(), pChildShellItem);
                m_pjld = pjld;
                int id = TrackPopupMenu(hMenu.get(), TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, *this, nullptr);
                DoJumpListMenu(*this, pjld, id);
                m_pjld = nullptr;
            }
        }
    }
}

void RootWindow::OnHotKey(int idHotKey, UINT fuModifiers, UINT vk)
{
    switch (idHotKey)
    {
    case HK_OPEN:
        SetForegroundWindow(*this);
        ShowWindow(*this, SW_SHOWNORMAL);
        break;
    }
}

void RootWindow::OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu)
{
}

void RootWindow::OnMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem)
{
    if (lpMeasureItem->CtlID == 0 || lpMeasureItem->CtlType == ODT_MENU)
    {
        if (m_hImageListMenu && m_pjld)
        {
            const int index = JumpListMenuGetIcon(m_pjld, lpMeasureItem->itemID);
            if (index >= 0)
            {
                int cx = 0, cy = 0;
                CHECK(ImageList_GetIconSize(m_hImageListMenu, &cx, &cy));

                lpMeasureItem->itemWidth = std::max(lpMeasureItem->itemWidth, UINT(cx));
                lpMeasureItem->itemHeight = std::max(lpMeasureItem->itemHeight, UINT(cy));
            }
        }

        if (lpMeasureItem->itemData)
        {
            LPCTSTR s = (LPCTSTR) lpMeasureItem->itemData;

            HDC hDC = GetDC(*this);
            SIZE sz = {};
            GetTextExtentPoint32(hDC, s, (int) _tcslen(s), &sz);
            ReleaseDC(*this, hDC);

            lpMeasureItem->itemWidth += sz.cx;
            lpMeasureItem->itemHeight = std::max<UINT>(lpMeasureItem->itemHeight, sz.cy);
        }
    }
}

HFONT CreateBoldFont(HFONT hFont)
{
    LOGFONT lf = {};
    GetObject(hFont, sizeof(LOGFONT), &lf);
    lf.lfWeight = FW_BOLD;
    return CreateFontIndirect(&lf);
}

void RootWindow::OnDrawItem(const DRAWITEMSTRUCT* lpDrawItem)
{
    if (lpDrawItem->CtlID == 0 || lpDrawItem->CtlType == ODT_MENU)
    {
        if (m_hImageListMenu && m_pjld)
        {
            const int index = JumpListMenuGetIcon(m_pjld, lpDrawItem->itemID);
            if (index >= 0)
                CHECK(ImageList_Draw(m_hImageListMenu, index, lpDrawItem->hDC, lpDrawItem->rcItem.left, lpDrawItem->rcItem.top, ILD_TRANSPARENT | (lpDrawItem->itemState & ODA_SELECT ? ILD_SELECTED : ILD_NORMAL)));
        }

        if (lpDrawItem->itemData)
        {
            static HFONT hFontBold = CreateBoldFont((HFONT) GetCurrentObject(lpDrawItem->hDC, OBJ_FONT));

            HFONT hOldFont = (HFONT) SelectObject(lpDrawItem->hDC, hFontBold);

            LPCTSTR s = (LPCTSTR) lpDrawItem->itemData;
            RECT rc = lpDrawItem->rcItem;
            DrawText(lpDrawItem->hDC, s, -1, &rc, DT_LEFT | DT_VCENTER);

            SelectObject(lpDrawItem->hDC, hOldFont);
        }
    }
}

LRESULT RootWindow::HandleMessage(const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    LRESULT ret = 0;
    switch (uMsg)
    {
        HANDLE_MSG(WM_CREATE, OnCreate);
        HANDLE_MSG(WM_DESTROY, OnDestroy);
        HANDLE_MSG(WM_SIZE, OnSize);
        HANDLE_MSG(WM_SETFOCUS, OnSetFocus);
        HANDLE_MSG(WM_NOTIFY, OnNotify);
        HANDLE_MSG(WM_CONTEXTMENU, OnContextMenu);
        HANDLE_MSG(WM_HOTKEY, OnHotKey);

        HANDLE_MSG(WM_INITMENUPOPUP, OnInitMenuPopup);
        HANDLE_MSG(WM_MEASUREITEM, OnMeasureItem);
        HANDLE_MSG(WM_DRAWITEM, OnDrawItem);
    }

    if (!IsHandled())
        ret = Window::HandleMessage(uMsg, wParam, lParam);

    return ret;
}

void RootWindow::Refresh()
{
    if (!m_pFolder)
        return;

    ListView_DeleteAllItems(m_hWndChild);
    while (ListView_GetGroupCount(m_hWndChild) > 0)
    {
        LVGROUP lvg = { sizeof(LVGROUP) };
        lvg.mask = LVGF_GROUPID;
        ListView_GetGroupInfoByIndex(m_hWndChild, 0, &lvg);
        ListView_RemoveGroup(m_hWndChild, lvg.iGroupId);
    }

    CComQIPtr<IShellIcon> pShellIcon(m_pFolder);

    CComPtr<IEnumIDList> pEnumIDList;
    CHECK_HR(m_pFolder->EnumObjects(*this, SHCONTF_FOLDERS | SHCONTF_INIT_ON_FIRST_NEXT, &pEnumIDList));
    int iGroupId = 0;
    CComHeapPtr<ITEMIDLIST> pIdList;
    while (pIdList.Free(), pEnumIDList->Next(1, &pIdList, nullptr) == S_OK)
    {
        CStrRet pName;
        CHECK_HR(m_pFolder->GetDisplayNameOf(pIdList, 0, &pName));
        CString name = pName.toStr(pIdList);

        int nIconIndex = 0;
        CHECK_HR(pShellIcon->GetIconOf(pIdList, GIL_FORSHELL, &nIconIndex));

        {
            LVGROUP lvg = { sizeof(LVGROUP) };
            lvg.mask = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE | LVGF_TITLEIMAGE;
            lvg.state = LVGS_COLLAPSIBLE;
            lvg.pszHeader = const_cast<LPWSTR>(LPCWSTR(name));
            lvg.iTitleImage = nIconIndex;
            lvg.iGroupId = ++iGroupId;
            CHECK(ListView_InsertGroup(m_hWndChild, -1, &lvg) >= 0);
        }

        CComQIPtr<IShellFolder> pChildShellFolder;
        CHECK_HR(m_pFolder->BindToObject(pIdList, nullptr, IID_PPV_ARGS(&pChildShellFolder)));
        if (pChildShellFolder)
        {
            CComQIPtr<IShellIcon> pChildShellIcon(pChildShellFolder);

            CComPtr<IEnumIDList> pChildEnumIDList;
            CHECK_HR(pChildShellFolder->EnumObjects(*this, SHCONTF_NONFOLDERS | SHCONTF_INIT_ON_FIRST_NEXT, &pChildEnumIDList));
            CComHeapPtr<ITEMIDLIST> pChildIdList;
            while (pChildIdList.Free(), pChildEnumIDList->Next(1, &pChildIdList, nullptr) == S_OK)
            {
                CComPtr<IShellItem> pChildShellItem;
                CHECK_HR(SHCreateShellItem(nullptr, pChildShellFolder, pChildIdList, &pChildShellItem));

                CStrRet pChildName;
                CHECK_HR(pChildShellFolder->GetDisplayNameOf(pChildIdList, 0, &pChildName));
                CString childname = pChildName.toStr(pChildIdList);

                int nChildIconIndex = 0;
                CHECK_HR(pChildShellIcon->GetIconOf(pChildIdList, GIL_FORSHELL, &nChildIconIndex));

                {
                    LVITEM item = {};
                    item.iItem = INT_MAX;
                    item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_GROUPID | LVIF_PARAM;
                    item.pszText = const_cast<LPWSTR>(LPCWSTR(childname));
                    item.iImage = nChildIconIndex;
                    item.iGroupId = iGroupId;
                    item.lParam = (LPARAM) pChildShellItem.Detach();
                    ListView_InsertItem(m_hWndChild, &item);
                }
            }
        }
    }
}

bool Run(_In_ const LPCTSTR lpCmdLine, _In_ const int nShowCmd)
{
    RadLogInitWnd(NULL, APPNAMEA, APPNAMEW);

    if (true)
    {
        CHECK_LE_RET(RootWindow::Register(), false);

        RootWindow* prw = RootWindow::Create();
        CHECK_LE_RET(prw != nullptr, false);

        RadLogInitWnd(*prw, APPNAMEA, APPNAMEW);
        ShowWindow(*prw, nShowCmd);
    }
    return true;
}
