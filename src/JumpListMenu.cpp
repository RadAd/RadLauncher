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

#include <map>
#include <set>

template<class K, class V>
const V& GetOr(const std::map<K, V>& map, const K& k, const V& v = {})
{
    auto it = map.find(k);
    if (it != map.end())
        return it->second;
    else
        return v;
}

void AppendMenuHeader(HMENU hMenu, CString name)
{
    if (GetMenuItemCount(hMenu) > 0)
        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    CHECK(AppendMenu(hMenu, MF_OWNERDRAW | MF_GRAYED | MF_DISABLED, 0, _wcsdup(name))); // TODO not deleting this wcsdup yet
}

struct JumpListData
{
    UINT minid;
    std::map<UINT, CComPtr<IUnknown>> objects;
    std::map<UINT, HICON> icons;
};

struct Data
{
    HMENU hMenu;
    UINT id;
    JumpListData* pData;
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
                    AppendMenu(data.hMenu, MF_STRING | MF_ENABLED, data.id++, pName);
                    SetMenuItemBitmap(data.hMenu, data.id - 1, FALSE, HBMMENU_CALLBACK);
                    data.pData->objects[data.id - 1] = pUnknown;
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
                        data.pData->objects[data.id - 1] = pUnknown;

                        {
                            WCHAR iconloc[1024] = TEXT("");
                            int iconindex = 0;
                            if (SUCCEEDED(pLink->GetIconLocation(iconloc, ARRAYSIZE(iconloc), &iconindex)) && iconloc[0] != TEXT('\0'))
                            {
                                HICON hIcon = NULL;
                                ExtractIconEx(iconloc, iconindex, NULL, &hIcon, 1);
                                if (hIcon)
                                {
                                    data.pData->icons[data.id - 1] = hIcon;
                                    SetMenuItemBitmap(data.hMenu, data.id - 1, FALSE, HBMMENU_CALLBACK);
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
                                SetMenuItemBitmap(data.hMenu, data.id - 1, FALSE, hBitmap);
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
                                    SetMenuItemBitmap(data.hMenu, data.id - 1, FALSE, hBitmap); // TODO Does the menu delete the bitmap
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

void FillJumpListMenu(HMENU hMenu, JumpListData* pjld, LPCWSTR pAppId)
{
    Data data = { hMenu, pjld->minid, pjld, pAppId };

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

JumpListData* FillJumpListMenu(HMENU hMenu, IShellItem* pShellItem)
{
    auto pjld = std::make_unique<JumpListData>();
    pjld->minid = 1;

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
            FillJumpListMenu(hMenu, pjld.get(), pAppId);
        }
        else
        {
            CComPtr<IPropertyStore> pStore;
            CHECK_HR(pShellItem->BindToHandler(NULL, BHID_PropertyStore, IID_PPV_ARGS(&pStore)));

            CString TheAppId = GetPropertyStoreString(pStore, PKEY_Link_TargetParsingPath);
            if (!TheAppId.IsEmpty())
                FillJumpListMenu(hMenu, pjld.get(), TheAppId);
        }
    }
    else
    {
        if (SUCCEEDED(pShellItem->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pAppId)))
        {
            FillJumpListMenu(hMenu, pjld.get(), pAppId);
        }
    }

    return pjld.release();
}

void DoJumpListMenu(HWND hWnd, JumpListData* pData, int id)
{
    std::unique_ptr<JumpListData> to_delete(pData);

    for (auto& i : pData->icons)
        DestroyIcon(i.second);

    CComPtr<IUnknown> pUnknown = GetOr<UINT>(pData->objects, id);
    if (pUnknown)
    {
        CComQIPtr<IShellItem> pItem(pUnknown);
        if (pItem)
        {
            CComPtr<IContextMenu> pContextMenu;
            CHECK_HR(pItem->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&pContextMenu)));
            if (pContextMenu)
                OpenDefaultItem(hWnd, pContextMenu);
        }

        CComQIPtr<IShellLink> pLink(pUnknown);
        if (pLink)
        {
            CComQIPtr<IContextMenu> pContextMenu(pLink);

            CComQIPtr<IPropertyStore> pStore(pLink);
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
}

int JumpListMenuGetSystemIcon(JumpListData* pData, int id)
{
    CComPtr<IUnknown> pUnknown = GetOr<UINT>(pData->objects, id);
    if (pUnknown)
    {
        CComQIPtr<IShellItem> pItem(pUnknown);
        if (pItem)
            return GetIconIndex(pItem);
    }

    return -1;
}

HICON JumpListMenuGetIcon(JumpListData* pData, int id)
{
    return GetOr<UINT>(pData->icons, id);
}
