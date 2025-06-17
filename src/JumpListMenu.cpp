#define STRICT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "JumpListMenu.h"
#include "JumpList.h"
#include "ShellUtils.h"
#include "ImageUtils.h"
#include "WindowsUtils.h"

#include "Rad/Log.h"
#include "Rad/MemoryPlus.h"

#include <atlcomcli.h>
#include <atlstr.h>
#include <propkey.h>

#include <ShlGuid.h>
#include <shellapi.h>

#include <set>

class MenuExecute
{
public:
    virtual ~MenuExecute() {}

    virtual void Execute(HWND hWnd) = 0;
};

class MenuExecuteIShellItem : public MenuExecute
{
public:
    class MenuExecuteIShellItem(IShellItem* pIShellItem) : m_pIShellItem(pIShellItem) {}

    virtual void Execute(HWND hWnd) override
    {
        if (m_pIShellItem)
        {
            CComPtr<IContextMenu> pContextMenu;
            CHECK_HR(m_pIShellItem->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&pContextMenu)));
            if (pContextMenu)
                OpenDefaultItem(hWnd, pContextMenu);
        }
    }

protected:
    IShellItem* GetShellItem() const
    {
        return m_pIShellItem;
    }

private:
    CComPtr<IShellItem> m_pIShellItem;
};

class MenuExecuteIShellLink : public MenuExecute
{
public:
    class MenuExecuteIShellLink(IShellLink* pIShellLink) : m_pIShellLink(pIShellLink) {}

    virtual void Execute(HWND hWnd) override
    {
        if (m_pIShellLink)
        {
            CComQIPtr<IContextMenu> pContextMenu(m_pIShellLink);

            CComQIPtr<IPropertyStore> pStore(m_pIShellLink);
            const CString params = GetPropertyStoreString(pStore, PKEY_AppUserModel_ActivationContext);
            const CString appId = GetPropertyStoreString(pStore, PKEY_AppUserModel_ID);
            if (!appId.IsEmpty())
            {
                CComPtr<IShellItem2> target;
                if (SUCCEEDED(SHCreateItemInKnownFolder(FOLDERID_AppsFolder, 0, appId, IID_PPV_ARGS(&target))))
                {
                    ULONG modern = 0;
                    if (SUCCEEDED(target->GetUInt32(PKEY_AppUserModel_HostEnvironment, &modern)) && modern)
                    {
                        CComQIPtr<IContextMenu> targetMenu;
                        if (SUCCEEDED(target->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&targetMenu))))
                        {
                            pContextMenu = targetMenu;
                        }
                    }
                }
            }

            if (pContextMenu)
                OpenDefaultItem(hWnd, pContextMenu, params);
        }
    }

protected:
    IShellLink* GetShellLink() const
    {
        return m_pIShellLink;
    }

private:
    CComPtr<IShellLink> m_pIShellLink;
};

class OwnerDrawnMenuString : public OwnerDrawnMenuItem
{
public:
    OwnerDrawnMenuString(CString s) : m_s(s) {}

    virtual void OnMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem) override
    {
        ATLASSERT(lpMeasureItem->CtlID == 0 && lpMeasureItem->CtlType == ODT_MENU);

        const auto hDC = AutoGetDC(NULL);
        SIZE sz = {};
        CHECK_LE(GetTextExtentPoint32(hDC.get(), m_s, m_s.GetLength(), &sz));
        sz.cy += 4;

        lpMeasureItem->itemWidth = sz.cx;
        lpMeasureItem->itemHeight = std::max<UINT>(lpMeasureItem->itemHeight, sz.cy);
    }

    virtual void OnDrawItem(const DRAWITEMSTRUCT* lpDrawItem) override
    {
        ATLASSERT(lpDrawItem->CtlID == 0 && lpDrawItem->CtlType == ODT_MENU);

        static HFONT hFontBold = CreateBoldFont((HFONT) GetCurrentObject(lpDrawItem->hDC, OBJ_FONT));

        const auto AutoFont = AutoSelectObject(lpDrawItem->hDC, hFontBold);

        RECT rc = lpDrawItem->rcItem;
        DrawText(lpDrawItem->hDC, m_s, -1, &rc, DT_LEFT | DT_VCENTER);
    }

private:
    static HFONT CreateBoldFont(HFONT hFont)
    {
        LOGFONT lf = {};
        GetObject(hFont, sizeof(LOGFONT), &lf);
        lf.lfWeight = FW_BOLD;
        return CreateFontIndirect(&lf);
    }

    CString m_s;
};

class OwnerDrawnMenuShellItem : public OwnerDrawnMenuItem, public MenuExecuteIShellItem
{
public:
    OwnerDrawnMenuShellItem(HIMAGELIST hImageListMenu, IShellItem* pIShellItem)
        : MenuExecuteIShellItem(pIShellItem)
        , m_hImageListMenu(hImageListMenu)
    {}

    virtual void OnMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem) override
    {
        ATLASSERT(lpMeasureItem->CtlID == 0 && lpMeasureItem->CtlType == ODT_MENU);

        int cx = 0, cy = 0;
        CHECK_LE(ImageList_GetIconSize(m_hImageListMenu, &cx, &cy));

        lpMeasureItem->itemWidth = std::max(lpMeasureItem->itemWidth, UINT(cx));
        lpMeasureItem->itemHeight = std::max(lpMeasureItem->itemHeight, UINT(cy));
    }

    virtual void OnDrawItem(const DRAWITEMSTRUCT* lpDrawItem) override
    {
        ATLASSERT(lpDrawItem->CtlID == 0 && lpDrawItem->CtlType == ODT_MENU);

        if (m_index == -1)
            m_index = GetIconIndex(GetShellItem());
        if (m_index == -1)
            m_index = -2;

        if (m_index >= 0)
            CHECK(ImageList_Draw(m_hImageListMenu, m_index, lpDrawItem->hDC, lpDrawItem->rcItem.left, lpDrawItem->rcItem.top, ILD_TRANSPARENT | (lpDrawItem->itemState & ODA_SELECT ? ILD_SELECTED : ILD_NORMAL)));
    }

private:
    HIMAGELIST m_hImageListMenu;
    int m_index = -1;
};

class OwnerDrawnMenuIcon : public OwnerDrawnMenuItem, public MenuExecuteIShellLink
{
public:
    OwnerDrawnMenuIcon(IShellLink* pIShellLink, HICON hIcon) : MenuExecuteIShellLink(pIShellLink), m_hIcon(hIcon) {}

    ~OwnerDrawnMenuIcon()
    {
        SetIcon(NULL);
    }

    void SetIcon(HICON hIcon)
    {
        if (m_hIcon)
            DestroyIcon(m_hIcon);
        m_hIcon = hIcon;
    }

    virtual void OnMeasureItem(MEASUREITEMSTRUCT* lpMeasureItem) override
    {
        ATLASSERT(lpMeasureItem->CtlID == 0 && lpMeasureItem->CtlType == ODT_MENU);

        if (m_hIcon != NULL)
        {
            ICONINFO ii = {};
            CHECK_LE(GetIconInfo(m_hIcon, &ii));
            BITMAP bm = {};
            GetObject(ii.hbmColor, sizeof(bm), &bm);
            DeleteObject(ii.hbmColor);
            DeleteObject(ii.hbmMask);

            lpMeasureItem->itemWidth = std::max(lpMeasureItem->itemWidth, UINT(bm.bmWidth));
            lpMeasureItem->itemHeight = std::max(lpMeasureItem->itemHeight, UINT(bm.bmHeight));
        }
    }

    virtual void OnDrawItem(const DRAWITEMSTRUCT* lpDrawItem) override
    {
        ATLASSERT(lpDrawItem->CtlID == 0 && lpDrawItem->CtlType == ODT_MENU);

        if (m_hIcon != NULL)
            DrawIconEx(lpDrawItem->hDC, lpDrawItem->rcItem.left, lpDrawItem->rcItem.top, m_hIcon, 0, 0, 0, NULL, DI_NORMAL);
    }

private:
    HICON m_hIcon;
};

void AppendMenuHeader(HMENU hMenu, CString name)
{
    if (GetMenuItemCount(hMenu) > 0)
        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    CHECK(AppendMenu(hMenu, MF_OWNERDRAW | MF_GRAYED | MF_DISABLED, 0, reinterpret_cast<LPCWSTR>(static_cast<OwnerDrawnMenuItem*>(new OwnerDrawnMenuString(name)))));
}

struct Data
{
    HMENU hMenu;
    HIMAGELIST hImageListMenu;
    UINT id;
    LPCWSTR pAppId;
};

struct CompareIShellItem
{
    bool operator()(IShellItem* p1, IShellItem* p2) const
    {
        int order = 0;
        p1->Compare(p2, SICHINT_ALLFIELDS, &order);
        return order < 0;
    }
};

void DoCollection(Data& data, IObjectCollection* pCollection, std::set<IShellItem*, CompareIShellItem>& ignore)
{
    UINT count;
    if (SUCCEEDED(pCollection->GetCount(&count)))
    {
        for (UINT i = 0; i < count; i++)
        {
            CComPtr<IUnknown> pUnknown;
            if (SUCCEEDED(pCollection->GetAt(i, IID_PPV_ARGS(&pUnknown))) && pUnknown)
            {
                CComQIPtr<IShellItem> pItem(pUnknown);
                if (pItem && (ignore.find(pItem) == ignore.end()))
                {
                    ignore.insert(pItem);

                    CComHeapPtr<WCHAR> pName;
                    CHECK_HR(pItem->GetDisplayName(SIGDN_NORMALDISPLAY, &pName));
                    CHECK_LE(AppendMenu(data.hMenu, MF_STRING | MF_ENABLED, data.id++, pName));
                    CHECK_LE(SetMenuItemData(data.hMenu, data.id - 1, FALSE, reinterpret_cast<ULONG_PTR>(static_cast<OwnerDrawnMenuItem*>(new OwnerDrawnMenuShellItem(data.hImageListMenu, pItem)))));
                    CHECK_LE(SetMenuItemBitmap(data.hMenu, data.id - 1, FALSE, HBMMENU_CALLBACK));
                }

                CComQIPtr<IShellLink> pLink(pUnknown);
                if (pLink)
                {
                    CComQIPtr<IPropertyStore> pStore(pLink);
                    //DumpPropertyStore(pStore);

                    BOOL bSeparator = GetPropertyStoreBool(pStore, PKEY_AppUserModel_IsDestListSeparator);
                    if (bSeparator)
                        AppendMenu(data.hMenu, MF_SEPARATOR, 0, nullptr);
                    else
                    {
                        const CString str = LoadIndirectString(GetPropertyStoreString(pStore, PKEY_Title));

                        AppendMenu(data.hMenu, MF_STRING | MF_ENABLED, data.id++, str);
                        OwnerDrawnMenuIcon* pOdmi = new OwnerDrawnMenuIcon(pLink, NULL);
                        CHECK_LE(SetMenuItemData(data.hMenu, data.id - 1, FALSE, reinterpret_cast<ULONG_PTR>(static_cast<OwnerDrawnMenuItem*>(pOdmi))));

                        {
                            WCHAR iconloc[1024] = TEXT("");
                            int iconindex = 0;
                            if (SUCCEEDED(pLink->GetIconLocation(iconloc, ARRAYSIZE(iconloc), &iconindex)) && iconloc[0] != TEXT('\0'))
                            {
                                HICON hIcon = NULL;
                                ExtractIconEx(iconloc, iconindex, NULL, &hIcon, 1);
                                if (hIcon)
                                {
                                    pOdmi->SetIcon(hIcon);
                                    CHECK_LE(SetMenuItemBitmap(data.hMenu, data.id - 1, FALSE, HBMMENU_CALLBACK));
                                }
                            }
                        }

                        static const SIZE sz = { GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON) };

                        CString appId = GetPropertyStoreString(pStore, PKEY_AppUserModel_ID);
                        if (appId.IsEmpty())
                            appId = data.pAppId;
                        const CString icon = GetPropertyStoreString(pStore, PKEY_AppUserModel_DestListLogoUri);

                        CString packageName;
                        if (!appId.IsEmpty())
                        {
                            CComPtr<IShellItem2> target;
                            if (SUCCEEDED(SHCreateItemInKnownFolder(FOLDERID_AppsFolder, 0, appId, IID_PPV_ARGS(&target))))
                            {
                                ULONG modern = 0;
                                if (SUCCEEDED(target->GetUInt32(PKEY_AppUserModel_HostEnvironment, &modern)) && modern)
                                {
#if 0
                                    CComPtr<IPropertyStore> pStore3;
                                    target->BindToHandler(NULL, BHID_PropertyStore, IID_PPV_ARGS(&pStore3));
                                    //DumpPropertyStore(pStore3);

                                    packageName = GetPropertyStoreString(pStore3, PKEY_MetroPackageName);
#else
                                    CComHeapPtr<WCHAR> pn;
                                    if (SUCCEEDED(target->GetString(PKEY_MetroPackageName, &pn)))
                                        packageName = pn;
#endif
                                }
                            }
                        }

                        // ms-appdata   https://learn.microsoft.com/en-us/uwp/api/windows.storage.applicationdata.localfolder?view=winrt-22621
                        if (icon.Left(13) == TEXT("ms-appdata://"))
                        {
                            // TODO This is a workaround for now
                            CString appId2(appId);
                            appId2.Replace(TEXT("!App"), TEXT(""));

                            CString SubPath = icon.Mid(13);
                            SubPath.Replace(TEXT("/local"), TEXT(R"(\LocalState)"));
                            SubPath.Replace(TEXT("/"), TEXT(R"(\)"));

                            CString FilePath = ExpandEnvironmentStrings(TEXT(R"(%LOCALAPPDATA%\Packages\)")) + appId2 + SubPath;

                            HBITMAP hBitmap = LoadImageFile(FilePath, &sz, true, true);
                            if (hBitmap)
                            {
                                pOdmi->SetIcon(NULL);
                                CHECK_LE(SetMenuItemBitmap(data.hMenu, data.id - 1, FALSE, hBitmap));
                            }
                        }
                        else if (!packageName.IsEmpty())
                        {
                            CComPtr<IResourceManager> pResManager;
                            CHECK_HR(pResManager.CoCreateInstance(CLSID_ResourceManager));
                            CHECK_HR(pResManager->InitializeForPackage(packageName));

                            CComPtr<IResourceContext> pResContext;
                            CHECK_HR(pResManager->GetDefaultContext(IID_PPV_ARGS(&pResContext)));

                            // TODO Doesn't appear to be working
                            CHECK_HR(pResContext->SetTargetSize((WORD) sz.cx));
                            CHECK_HR(pResContext->SetScale(RES_SCALE_80));
                            CHECK_HR(pResContext->SetContrast(RES_CONTRAST_WHITE));

                            CComPtr<IResourceMap> pResMap;
                            CHECK_HR(pResManager->GetMainResourceMap(IID_PPV_ARGS(&pResMap)));

                            CComHeapPtr<WCHAR> pFilePath;
                            if (SUCCEEDED(pResMap->GetFilePath(icon, &pFilePath)))
                            //if (SUCCEEDED(pResMap->GetFilePathForContext(pResContext, icon, &pFilePath)))
                            {
                                HBITMAP hBitmap = LoadImageFile(pFilePath, &sz, true, true);
                                if (hBitmap)
                                {
                                    pOdmi->SetIcon(NULL);
                                    CHECK_LE(SetMenuItemBitmap(data.hMenu, data.id - 1, FALSE, hBitmap)); // TODO Does the menu delete the bitmap
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void DoCollection(Data& data, IObjectCollection* pCollection, CategoryType listType, std::set<IShellItem*, CompareIShellItem>& ignore)
{
    UINT count = 0;
    CHECK_HR(pCollection->GetCount(&count));

    if (count > 0)
    {
        static LPCWSTR names[] = { L"Pinned", L"Recent", L"Frequent", L"Other1", L"Other2" };
        AppendMenuHeader(data.hMenu, names[listType]);
        DoCollection(data, pCollection, ignore);
    }
}

void DoList(Data& data, IAutomaticDestinationList* m_pAutoList, IAutomaticDestinationList10b* m_pAutoList10b, CategoryType listType, std::set<IShellItem*, CompareIShellItem>& ignore)
{
    BOOL bHasList = FALSE;
    if (m_pAutoList)
        CHECK_HR(m_pAutoList->HasList(&bHasList));
    if (m_pAutoList10b)
        CHECK_HR(m_pAutoList10b->HasList(&bHasList));

    if (bHasList)
    {
        const unsigned int maxCount = 10;

        CComPtr<IObjectCollection> pCollection;
        if (m_pAutoList)
            m_pAutoList->GetList(listType, maxCount, IID_PPV_ARGS(&pCollection));
        if (m_pAutoList10b)
            m_pAutoList10b->GetList(listType, maxCount, 0, IID_PPV_ARGS(&pCollection));

        if (pCollection)
            DoCollection(data, pCollection, listType, ignore);
    }
}

void DoCustomList(Data& data, IDestinationList* pCustomList, UINT index, LPCWSTR name, std::set<IShellItem*, CompareIShellItem>& ignore)
{
    AppendMenuHeader(data.hMenu, name);

    CComPtr<IObjectCollection> pCollection;
    CHECK_HR(pCustomList->EnumerateCategoryDestinations(index, IID_PPV_ARGS(&pCollection)));

    DoCollection(data, pCollection, ignore);
}

void FillJumpListMenu(HMENU hMenu, HIMAGELIST hImageListMenu, LPCWSTR pAppId)
{
    Data data = { hMenu, hImageListMenu, 1, pAppId };

    CComPtr<IAutomaticDestinationList> m_pAutoList;
    CComPtr<IAutomaticDestinationList10b> m_pAutoList10b;
    {
        CComPtr<IUnknown> pAutoListUnk;
        CHECK_HR(pAutoListUnk.CoCreateInstance(CLSID_AutomaticDestinationList));

        if (!m_pAutoList)
            m_pAutoList = CComQIPtr<IAutomaticDestinationList>(pAutoListUnk);
        if (!m_pAutoList10b)
            m_pAutoList10b = CComQIPtr<IAutomaticDestinationList10b>(pAutoListUnk);

        if (m_pAutoList)
            CHECK_HR(m_pAutoList->Initialize(pAppId, NULL, NULL));
        if (m_pAutoList10b)
            CHECK_HR(m_pAutoList10b->Initialize(pAppId, NULL, NULL));
    }

    std::set<IShellItem*, CompareIShellItem> ignore;
    DoList(data, m_pAutoList, m_pAutoList10b, TYPE_PINNED, ignore);

    CComPtr<IDestinationList> pCustomList;
    {
        CComPtr<IUnknown> pCustomListUnk;
        CHECK_HR(pCustomListUnk.CoCreateInstance(CLSID_DestinationList));

        if (!pCustomList)
            pCustomList = CComQIPtr<IDestinationList, &IID_IDestinationList>(pCustomListUnk);
        if (!pCustomList)
            pCustomList = CComQIPtr<IDestinationList, &IID_IDestinationList10a>(pCustomListUnk);
        if (!pCustomList)
            pCustomList = CComQIPtr<IDestinationList, &IID_IDestinationList10b>(pCustomListUnk);
    }

    UINT categoryCount = 0;
    if (pCustomList)
    {
        CHECK_HR(pCustomList->SetApplicationID(pAppId));

        CHECK_HR(pCustomList->GetCategoryCount(&categoryCount));

        int tasks = -1;
        for (UINT catIndex = 0; catIndex < categoryCount; catIndex++)
        {
            APPDESTCATEGORY category = {};
            if (SUCCEEDED(pCustomList->GetCategory(catIndex, 1, &category)))
            {
                switch (category.type)
                {
                case 0:
                    DoCustomList(data, pCustomList, catIndex, LoadIndirectString(category.name), ignore);
                    CoTaskMemFree(category.name);
                    break;

                case 1:
                    // category.subType 1 == "Frequent"
                    // category.subType 2 == "Recent"
                    if (category.subType >= 1 && category.subType <= 2)
                        DoList(data, m_pAutoList, m_pAutoList10b, CategoryType(3 - category.subType), ignore);
                    break;

                case 2:
                    ATLASSERT(tasks == -1);
                    tasks = catIndex;
                    break;

                default:
                    ATLASSERT(FALSE);
                    break;
                }
            }
        }

        if (tasks != -1)
            DoCustomList(data, pCustomList, tasks, L"Tasks", ignore);
    }

    if (categoryCount == 0)
        DoList(data, m_pAutoList, m_pAutoList10b, TYPE_RECENT, ignore);
}

void FillJumpListMenu(HMENU hMenu, HIMAGELIST hImageListMenu, IShellItem* pShellItem)
{
    CComPtr<IApplicationResolver> pAppResolver;
    {
        CComPtr<IUnknown> pUnknown;
        CHECK_HR(pUnknown.CoCreateInstance(CLSID_ApplicationResolver));

        if (!pAppResolver)
            pAppResolver = CComQIPtr<IApplicationResolver, &IID_IApplicationResolver7>(pUnknown);
        if (!pAppResolver)
            pAppResolver = CComQIPtr<IApplicationResolver, &IID_IApplicationResolver8>(pUnknown);
    }

    CComHeapPtr<WCHAR> pAppId;

    CComPtr<IShellLink> pShellLink;
    if (SUCCEEDED(pShellItem->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&pShellLink))))
    {
        if (SUCCEEDED(pAppResolver->GetAppIDForShortcut(pShellItem, &pAppId)))
        {
            FillJumpListMenu(hMenu, hImageListMenu, pAppId);
        }
        else
        {
            CComPtr<IPropertyStore> pStore;
            CHECK_HR(pShellItem->BindToHandler(NULL, BHID_PropertyStore, IID_PPV_ARGS(&pStore)));

            CString TheAppId = GetPropertyStoreString(pStore, PKEY_Link_TargetParsingPath);
            if (!TheAppId.IsEmpty())
                FillJumpListMenu(hMenu, hImageListMenu, TheAppId);
        }
    }
    else
    {
        if (SUCCEEDED(pShellItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pAppId)))
        {
            FillJumpListMenu(hMenu, hImageListMenu, pAppId);
        }
    }
}

void DoJumpListMenu(HMENU hMenu, HWND hWnd, int id)
{
    ULONG_PTR p = 0;
    CHECK_LE(GetMenuItemData(hMenu, id, FALSE, p));

    if (p)
    {
        OwnerDrawnMenuItem* podm = reinterpret_cast<OwnerDrawnMenuItem*>(p);
        MenuExecute* pme = dynamic_cast<MenuExecute*>(podm);
        if (pme)
            pme->Execute(hWnd);
    }

    const int count = GetMenuItemCount(hMenu);
    for (int i = 0; i < count; ++i)
    {
        CHECK_LE(GetMenuItemData(hMenu, i, TRUE, p));
        OwnerDrawnMenuItem* podm = reinterpret_cast<OwnerDrawnMenuItem*>(p);
        if (podm)
            delete podm;
    }
}
