#include "Rad/Window.h"
#include "Rad/Windowxx.h"
#include "Rad/Log.h"
#include "Rad/ListViewPlus.h"
#include "Rad/MemoryPlus.h"
#include "Rad/Format.h"
#include "Rad/WindowsPlus.h"

#include <ShlObj.h>
#include <shellapi.h>
#include <commoncontrols.h>

#include "ShellUtils.h"
#include "WindowsUtils.h"
#include "JumpListMenu.h"

#include "..\resource.h"

extern HINSTANCE g_hInstance;
extern HACCEL g_hAccelTable;
extern HWND g_hWndAccel;

#define APPNAME TEXT("RadLauncher")
#define APPNAMEA "RadLauncher"
#define APPNAMEW L"RadLauncher"

//#include <tchar.h>
//#include <strsafe.h>
//#include "resource.h"

#define ID_LIST                         101

#define HK_OPEN                 1

HIMAGELIST MyGetImageList(int imagelist, SrcLoc src)
{
    IImageList* pImageList = nullptr;
    //CHECK_HR(SHGetImageList(imagelist, IID_PPV_ARGS(&pImageList)));
    if (FAILED(g_radloghr = SHGetImageList(imagelist, IID_PPV_ARGS(&pImageList))))
        RadLog(LOG_ASSERT, WinError::getMessage(g_radloghr, nullptr, TEXT(__FUNCTION__)), src);
    return (HIMAGELIST) pImageList;
}

bool IsDigit(WCHAR ch)
{
    WORD type = {};
    return GetStringTypeW(CT_CTYPE1, &ch, 1, &type) && (type & C1_DIGIT);
}

class RootWindow : public Window
{
    friend WindowManager<RootWindow>;
public:
    static bool IsExisting() { return FindWindow(ClassName(), nullptr) != NULL; }
    static ATOM Register() { return WindowManager<RootWindow>::Register(); }
    static RootWindow* Create() { return WindowManager<RootWindow>::Create(); }

protected:
    static void GetWndClass(WNDCLASS& wc);
    static void GetCreateWindow(CREATESTRUCT& cs);

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

private:
    BOOL OnCreate(LPCREATESTRUCT lpCreateStruct);
    void OnClose();
    void OnDestroy();
    void OnSize(UINT state, int cx, int cy);
    void OnSetFocus(HWND hwndOldFocus);
    LRESULT OnNotify(const DWORD dwID, const LPNMHDR pNmHdr);
    void OnContextMenu(HWND hWndContext, UINT xPos, UINT yPos);
    void OnHotKey(int idHotKey, UINT fuModifiers, UINT vk);
    void OnActivate(UINT state, HWND hWndActDeact, BOOL fMinimized);
    void OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu);
    void OnMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem);
    void OnDrawItem(const DRAWITEMSTRUCT* lpDrawItem);
    void OnCommand(int id, HWND hWndCtl, UINT codeNotify);

    static LPCTSTR ClassName() { return APPNAME; }

    void Refresh();

    CComPtr<IShellFolder> m_pFolder;
    int m_iFolderIcon = 0;
    //LPITEMIDLIST m_pIdList;
    HWND m_hWndChild = NULL;
    HIMAGELIST m_hImageListMenu = NULL;
    JumpListData* m_pjld = nullptr;

    bool m_HideOnLaunch = true;
};

void RootWindow::GetWndClass(WNDCLASS& wc)
{
    Window::GetWndClass(wc);
    wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
}

void RootWindow::GetCreateWindow(CREATESTRUCT& cs)
{
    Window::GetCreateWindow(cs);
    cs.lpszName = APPNAME;
    cs.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
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

    m_hWndChild = ListView_Create(*this, RECT(), WS_CHILD | WS_VISIBLE | LVS_ALIGNTOP | LVS_ICON | LVS_SINGLESEL | LVS_SHAREIMAGELISTS | LVS_AUTOARRANGE, LVS_EX_FLATSB | LVS_EX_TWOCLICKACTIVATE, ID_LIST);
    CHECK_LE(m_hWndChild);
    CHECK(ListView_EnableGroupView(m_hWndChild, TRUE) >= 0);
    ListView_SetImageList(m_hWndChild, hImageListLg, LVSIL_NORMAL);
    ListView_SetImageList(m_hWndChild, hImageListSm, LVSIL_SMALL);
    ListView_SetGroupHeaderImageList(m_hWndChild, hImageListSm);
    //ListView_SetIconSpacing(m_hWndChild, 80, 0);
    //ListView_SetExtendedListViewStyle(m_hWndChild, LVS_EX_TWOCLICKACTIVATE);
    //ListView_SetView(m_hWndChild, LV_VIEW_ICON);

    ListView_SetBkColor(m_hWndChild, QueryRegDWORDValue(reg, TEXT("bkcolor"), RGB(0, 0, 0)));
    ListView_SetTextColor(m_hWndChild, QueryRegDWORDValue(reg, TEXT("textcolor"), RGB(250, 250, 250)));
    ListView_SetTextBkColor(m_hWndChild, QueryRegDWORDValue(reg, TEXT("textbkcolor"), RGB(61, 61, 61)));
    //ListView_SetOutlineColor(m_hWndChild, QueryRegDWORDValue(reg, TEXT("outlinekcolor"), RGB(255, 0, 255)));

    //const CString folderspec = IsDebuggerPresent() ? LR"(%__APPDIR__%..\..\Menu)" : RegQueryStringValue(reg, TEXT("folder"), TEXT(R"(%LOCALAPPDATA%\)") APPNAME TEXT(R"(\Menu)"));
    const CString folderspec = RegQueryStringValue(reg, TEXT("folder"), TEXT(R"(%LOCALAPPDATA%\)") APPNAME TEXT(R"(\Menu)"));
    const CString folder = PathCanonicalize(ExpandEnvironmentStrings(folderspec));
    //const CString folder = TEXT("shell:appsfolder");

    CComPtr<IShellFolder> pDesktopFolder;
    CHECK_HR(SHGetDesktopFolder(&pDesktopFolder));
    CComHeapPtr<ITEMIDLIST> pIdList;
    //CHECK_HR(SHGetKnownFolderIDList(FOLDERID_StartMenu, KF_FLAG_DEFAULT, NULL, &pIdList));
    //CHECK_HR(SHGetKnownFolderIDList(FOLDERID_Programs, KF_FLAG_DEFAULT, NULL, &pIdList));
    //CHECK_HR(SHGetKnownFolderIDList(FOLDERID_CommonPrograms, KF_FLAG_DEFAULT, NULL, &pIdList));
    if (FAILED(SHParseDisplayName(folder, nullptr, &pIdList, 0, nullptr)))
        MessageBox(*this, Format(TEXT("Unable to open folder \"%s\""), folder.GetString()).c_str(), APPNAME, MB_ICONERROR | MB_OK);
    else
        CHECK_HR(pDesktopFolder->BindToObject(pIdList, nullptr, IID_PPV_ARGS(&m_pFolder)));
    m_iFolderIcon = GetIconIndex(pDesktopFolder, pIdList);

    Refresh();

    CHECK_LE(RegisterHotKey(*this, HK_OPEN, MOD_CONTROL | MOD_ALT, 'L'));
    //::SendMessage(*this, WM_SETHOTKEY, MAKEWORD('L', HOTKEYF_CONTROL | HOTKEYF_ALT), 0);

    return TRUE;
}

void RootWindow::OnClose()
{
    if (!m_HideOnLaunch || GetKeyState(VK_SHIFT) & KF_UP)
        SetHandled(false);
    else
        ShowWindow(*this, SW_HIDE);
}

void RootWindow::OnDestroy()
{
    CRegKey reg;
    reg.Create(HKEY_CURRENT_USER, TEXT(R"(Software\RadSoft\)") APPNAME);

    if (reg)
    {
        RECT rc = {};
        CHECK_LE(GetWindowRect(*this, &rc));

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

            case VK_ESCAPE:
                if (m_HideOnLaunch)
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
                    if (m_HideOnLaunch)
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
            if (xPos == UINT_MAX && yPos == UINT_MAX)
            {
                RECT rc = {};
                ListView_GetItemRect(m_hWndChild, item.iItem, &rc, LVIR_SELECTBOUNDS);
                pt.x = rc.right;
                pt.y = rc.top;
                CHECK_LE(ClientToScreen(m_hWndChild, &pt));
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
                if (id > 0 && m_HideOnLaunch)
                    ShowWindow(*this, SW_HIDE);
                m_pjld = nullptr;
            }
        }
        else
        {
            const auto hMenu = MakeUniqueHandle(LoadPopupMenu(g_hInstance, IDR_MENU1), DestroyMenu);
            TrackPopupMenu(hMenu.get(), TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, xPos, yPos, 0, *this, nullptr);
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

void RootWindow::OnActivate(UINT state, HWND hWndActDeact, BOOL fMinimized)
{
    if (state == WA_INACTIVE)
    {
        g_hWndAccel = NULL;
        g_hAccelTable = NULL;
    }
    else
    {
        g_hWndAccel = *this;
        g_hAccelTable = LoadAccelerators(g_hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));
    }
}

void RootWindow::OnInitMenuPopup(HMENU hMenu, UINT item, BOOL fSystemMenu)
{
    if (!fSystemMenu)
    {
        const UINT id = GetMenuItemID(hMenu, 0);
        switch (id)
        {
        case ID_VIEW_EXTRA:
        {
            UINT cmd = 0;
            const UINT view = ListView_GetView(m_hWndChild);
            switch (view)
            {
            case LV_VIEW_ICON:
            {
                HIMAGELIST hImageList = ListView_GetImageList(m_hWndChild, LVSIL_NORMAL);
                if (hImageList == MyGetImageList(SHIL_JUMBO, SRC_LOC))
                    cmd = ID_VIEW_EXTRA;
                else if (hImageList == MyGetImageList(SHIL_EXTRALARGE, SRC_LOC))
                    cmd = ID_VIEW_LARGEICONS;
                else if (hImageList == MyGetImageList(SHIL_LARGE, SRC_LOC))
                    cmd = ID_VIEW_MEDIUM;
                break;
            }
            case LV_VIEW_SMALLICON:
                cmd = ID_VIEW_SMALLICONS;
                break;
            case LV_VIEW_LIST:
                cmd = ID_VIEW_LIST;
                break;
            case LV_VIEW_DETAILS:
                cmd = ID_VIEW_DETAILS;
                break;
            case LV_VIEW_TILE:
                cmd = ID_VIEW_TILES;
                break;
            }

            CheckMenuRadioItem(hMenu, ID_VIEW_EXTRA, ID_VIEW_TILES, cmd, MF_BYCOMMAND);
        }
        break;

        case ID_SETTINGS_HIDEONLAUNCH:
            CheckMenuItem(hMenu, ID_SETTINGS_HIDEONLAUNCH, MF_BYCOMMAND | (m_HideOnLaunch ? MF_CHECKED : MF_UNCHECKED));
            break;
        }
    }
}

void RootWindow::OnMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem)
{
    if (lpMeasureItem->CtlID == 0 || lpMeasureItem->CtlType == ODT_MENU)
    {
        if (m_hImageListMenu && m_pjld)
        {
            const int index = JumpListMenuGetSystemIcon(m_pjld, lpMeasureItem->itemID);
            if (index >= 0)
            {
                int cx = 0, cy = 0;
                CHECK(ImageList_GetIconSize(m_hImageListMenu, &cx, &cy));

                lpMeasureItem->itemWidth = std::max(lpMeasureItem->itemWidth, UINT(cx));
                lpMeasureItem->itemHeight = std::max(lpMeasureItem->itemHeight, UINT(cy));
            }

            const HICON hIcon = JumpListMenuGetIcon(m_pjld, lpMeasureItem->itemID);
            if (hIcon != NULL)
            {
                ICONINFO ii = {};
                CHECK_LE(GetIconInfo(hIcon, &ii));
                BITMAP bm = {};
                GetObject(ii.hbmColor, sizeof(bm), &bm);
                DeleteObject(ii.hbmColor);
                DeleteObject(ii.hbmMask);

                lpMeasureItem->itemWidth = std::max(lpMeasureItem->itemWidth, UINT(bm.bmWidth));
                lpMeasureItem->itemHeight = std::max(lpMeasureItem->itemHeight, UINT(bm.bmHeight));
            }
        }

        if (lpMeasureItem->itemData)
        {
            LPCTSTR s = (LPCTSTR) lpMeasureItem->itemData;

            const auto hDC = AutoGetDC(*this);
            SIZE sz = {};
            CHECK_LE(GetTextExtentPoint32(hDC.get(), s, (int) _tcslen(s), &sz));

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
            const int index = JumpListMenuGetSystemIcon(m_pjld, lpDrawItem->itemID);
            if (index >= 0)
                CHECK(ImageList_Draw(m_hImageListMenu, index, lpDrawItem->hDC, lpDrawItem->rcItem.left, lpDrawItem->rcItem.top, ILD_TRANSPARENT | (lpDrawItem->itemState & ODA_SELECT ? ILD_SELECTED : ILD_NORMAL)));

            const HICON hIcon = JumpListMenuGetIcon(m_pjld, lpDrawItem->itemID);
            if (hIcon != NULL)
                DrawIconEx(lpDrawItem->hDC, lpDrawItem->rcItem.left, lpDrawItem->rcItem.top, hIcon, 0, 0, 0, NULL, DI_NORMAL);
        }

        if (lpDrawItem->itemData)
        {
            static HFONT hFontBold = CreateBoldFont((HFONT) GetCurrentObject(lpDrawItem->hDC, OBJ_FONT));

            const auto AutoFont = AutoSelectObject(lpDrawItem->hDC, hFontBold);

            LPCTSTR s = (LPCTSTR) lpDrawItem->itemData;
            RECT rc = lpDrawItem->rcItem;
            DrawText(lpDrawItem->hDC, s, -1, &rc, DT_LEFT | DT_VCENTER);
        }
    }
}

void RootWindow::OnCommand(int id, HWND hWndCtl, UINT codeNotify)
{
    switch (id)
    {
    case ID_VIEW_EXTRA:
        ListView_SetImageList(m_hWndChild, MyGetImageList(SHIL_JUMBO, SRC_LOC), LVSIL_NORMAL);
        ListView_SetView(m_hWndChild, LV_VIEW_ICON);
        break;
    case ID_VIEW_LARGEICONS:
        ListView_SetImageList(m_hWndChild, MyGetImageList(SHIL_EXTRALARGE, SRC_LOC), LVSIL_NORMAL);
        ListView_SetView(m_hWndChild, LV_VIEW_ICON);
        break;
    case ID_VIEW_MEDIUM:
        ListView_SetImageList(m_hWndChild, MyGetImageList(SHIL_LARGE, SRC_LOC), LVSIL_NORMAL);
        ListView_SetView(m_hWndChild, LV_VIEW_ICON);
        break;
    case ID_VIEW_SMALLICONS:
        ListView_SetView(m_hWndChild, LV_VIEW_SMALLICON);
        break;
    case ID_VIEW_LIST:
        ListView_SetView(m_hWndChild, LV_VIEW_LIST);
        break;
    case ID_VIEW_DETAILS:
        ListView_SetView(m_hWndChild, LV_VIEW_DETAILS);
        break;
    case ID_VIEW_TILES:
        ListView_SetImageList(m_hWndChild, MyGetImageList(SHIL_EXTRALARGE, SRC_LOC), LVSIL_NORMAL);
        ListView_SetView(m_hWndChild, LV_VIEW_TILE);
        break;
    case ID_MAIN_REFRESH:
        Refresh();
        break;
    case ID_MAIN_COLLAPSEALLGROUPS:
    {
        const UINT count = (UINT) ListView_GetGroupCount(m_hWndChild);
        for (UINT i = 0; i < count; ++i)
        {
            LVGROUP lvg = { sizeof(LVGROUP) };
            lvg.mask = LVGF_GROUPID | LVGF_STATE;
            ListView_GetGroupInfoByIndex(m_hWndChild, i, &lvg);
            ListView_SetGroupState(m_hWndChild, lvg.iGroupId, LVGS_COLLAPSED, LVGS_COLLAPSED);
        }
        break;
    }
    case ID_MAIN_EXPANDALLGROUPS:
    {
        const UINT count = (UINT) ListView_GetGroupCount(m_hWndChild);
        for (UINT i = 0; i < count; ++i)
        {
            LVGROUP lvg = { sizeof(LVGROUP) };
            lvg.mask = LVGF_GROUPID | LVGF_STATE;
            ListView_GetGroupInfoByIndex(m_hWndChild, i, &lvg);
            ListView_SetGroupState(m_hWndChild, lvg.iGroupId, LVGS_COLLAPSED, 0);
        }
        break;
    }
    case ID_SETTINGS_HIDEONLAUNCH:
        m_HideOnLaunch = !m_HideOnLaunch;
        break;
    }
}

LRESULT RootWindow::HandleMessage(const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    LRESULT ret = 0;
    switch (uMsg)
    {
        HANDLE_MSG(WM_CREATE, OnCreate);
        HANDLE_MSG(WM_CLOSE, OnClose);
        HANDLE_MSG(WM_DESTROY, OnDestroy);
        HANDLE_MSG(WM_SIZE, OnSize);
        HANDLE_MSG(WM_SETFOCUS, OnSetFocus);
        HANDLE_MSG(WM_NOTIFY, OnNotify);
        HANDLE_MSG(WM_CONTEXTMENU, OnContextMenu);
        HANDLE_MSG(WM_HOTKEY, OnHotKey);
        HANDLE_MSG(WM_ACTIVATE, OnActivate);

        HANDLE_MSG(WM_INITMENUPOPUP, OnInitMenuPopup);
        HANDLE_MSG(WM_MEASUREITEM, OnMeasureItem);
        HANDLE_MSG(WM_DRAWITEM, OnDrawItem);

        HANDLE_MSG(WM_COMMAND, OnCommand);
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

    ListView_AddColumn(m_hWndChild, TEXT("Name"), LVCFMT_LEFT, 200);

    int iGroupId = 0;
    {
        LVGROUP lvg = { sizeof(LVGROUP) };
        lvg.mask = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE | LVGF_TITLEIMAGE;
        lvg.state = LVGS_COLLAPSIBLE;
        lvg.pszHeader = const_cast<LPWSTR>(L"Folder");
        lvg.iTitleImage = m_iFolderIcon;
        lvg.iGroupId = ++iGroupId;
        CHECK(ListView_InsertGroup(m_hWndChild, -1, &lvg) >= 0);
    }
    const int iMainGroupId = iGroupId;

    CComPtr<IEnumIDList> pEnumIDList;
    CHECK_HR(m_pFolder->EnumObjects(*this, SHCONTF_FOLDERS | SHCONTF_INIT_ON_FIRST_NEXT, &pEnumIDList));
    CComHeapPtr<ITEMIDLIST> pIdList;
    while (pIdList.Free(), pEnumIDList->Next(1, &pIdList, nullptr) == S_OK)
    {
        CStrRet pName;
        CHECK_HR(m_pFolder->GetDisplayNameOf(pIdList, 0, &pName));
        const CString name = pName.toStr(pIdList);
        // Remove leading digits followed by '.'
        int nameoffset = 0;
        while (nameoffset < name.GetLength() && IsDigit(name[nameoffset]))
            ++nameoffset;
        if (name[nameoffset] == TEXT('.'))
            ++nameoffset;
        else
            nameoffset = 0;

        int nIconIndex = GetIconIndex(m_pFolder, pIdList);

        CComPtr<IShellFolder> pChildShellFolder;
        /*CHECK_HR(m_pFolder->BindToObject(pIdList, nullptr, IID_PPV_ARGS(&pChildShellFolder)))*/;
        //if (pChildShellFolder)
        if (SUCCEEDED(m_pFolder->BindToObject(pIdList, nullptr, IID_PPV_ARGS(&pChildShellFolder))))
        {
            {
                LVGROUP lvg = { sizeof(LVGROUP) };
                lvg.mask = LVGF_HEADER | LVGF_GROUPID | LVGF_STATE | LVGF_TITLEIMAGE;
                lvg.state = LVGS_COLLAPSIBLE;
                lvg.pszHeader = const_cast<LPWSTR>(LPCWSTR(name) + nameoffset);
                lvg.iTitleImage = nIconIndex;
                lvg.iGroupId = ++iGroupId;
                CHECK(ListView_InsertGroup(m_hWndChild, -1, &lvg) >= 0);
            }

            CComPtr<IEnumIDList> pChildEnumIDList;
            CHECK_HR(pChildShellFolder->EnumObjects(*this, SHCONTF_NONFOLDERS | SHCONTF_INIT_ON_FIRST_NEXT, &pChildEnumIDList));
            CComHeapPtr<ITEMIDLIST> pChildIdList;
            while (pChildIdList.Free(), pChildEnumIDList->Next(1, &pChildIdList, nullptr) == S_OK)
            {
                CComPtr<IShellItem> pChildShellItem;
                CHECK_HR(SHCreateShellItem(nullptr, pChildShellFolder, pChildIdList, &pChildShellItem));

                CStrRet pChildName;
                CHECK_HR(pChildShellFolder->GetDisplayNameOf(pChildIdList, 0, &pChildName));
                const CString childname = pChildName.toStr(pChildIdList);

                int nChildIconIndex = GetIconIndex(pChildShellFolder, pChildIdList);

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
        else
        {
            CComPtr<IShellItem> pShellItem;
            CHECK_HR(SHCreateShellItem(nullptr, m_pFolder, pIdList, &pShellItem));

            {
                LVITEM item = {};
                item.iItem = INT_MAX;
                item.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_GROUPID | LVIF_PARAM;
                item.pszText = const_cast<LPWSTR>(LPCWSTR(name));
                item.iImage = nIconIndex;
                item.iGroupId = iMainGroupId;
                item.lParam = (LPARAM)pShellItem.Detach();
                ListView_InsertItem(m_hWndChild, &item);
            }
        }
    }
}

bool Run(_In_ const LPCTSTR lpCmdLine, _In_ const int nShowCmd)
{
    if (RootWindow::IsExisting())
    {
        MessageBox(NULL, TEXT("Process already exists."), APPNAME, MB_ICONERROR | MB_OK);
        return false;
    }

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
