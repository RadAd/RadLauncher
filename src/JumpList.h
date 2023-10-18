#pragma once

#include <ShObjIdl.h>

DEFINE_GUID(CLSID_ApplicationResolver, 0x660B90C8, 0x73A9, 0x4B58, 0x8C, 0xAE, 0x35, 0x5B, 0x7F, 0x55, 0x34, 0x1B); // "660B90C8-73A9-4B58-8CAE-355B7F55341B"
// different IIDs for Win8 and Win8: http://a-whiter.livejournal.com/1266.html
DEFINE_GUID(IID_IApplicationResolver7, 0x46A6EEFF, 0x908E, 0x4DC6, 0x92, 0xA6, 0x64, 0xBE, 0x91, 0x77, 0xB4, 0x1C);
DEFINE_GUID(IID_IApplicationResolver8, 0xDE25675A, 0x72DE, 0x44B4, 0x93, 0x73, 0x05, 0x17, 0x04, 0x50, 0xC1, 0x40);
//DEFINE_GUID(IID_IApplicationResolver8, 0xDE25675A, 0x72DE, 0x44B4, 0x93, 0x73, 0x05, 0x17, 0x04, 0x50, 0xC1, 0x40); // "DE25675A-72DE-44b4-9373-05170450C140"

interface IApplicationResolver : public IUnknown
{
    STDMETHOD(GetAppIDForShortcut)(IShellItem* psi, LPWSTR* ppszAppID);
    // .... we don't care about the rest of the methods ....
    STDMETHOD(GetAppIDForShortcutObject)() PURE; // not in v7
    STDMETHOD(GetAppIDForWindow)(HWND hWnd, LPWSTR* pszAppId, void* pUnknown1, void* pUnknown2, void* pUnknown3) PURE;
    STDMETHOD(GetAppIDForProcess)(DWORD dwProcessId, LPWSTR* pszAppId, void* pUnknown1, void* pUnknown2, void* pUnknown3) PURE;
};

DEFINE_GUID(IID_IDestinationList, 0x03F1EED2, 0x8676, 0x430B, 0xAB, 0xE1, 0x76, 0x5C, 0x1D, 0x8F, 0xE1, 0x47); // "03F1EED2-8676-430B-ABE1-765C1D8FE147"
DEFINE_GUID(IID_IDestinationList10a, 0xFEBD543D, 0x1F7B, 0x4B38, 0x94, 0x0B, 0x59, 0x33, 0xBD, 0x2C, 0xB2, 0x1B); // 10240
DEFINE_GUID(IID_IDestinationList10b, 0x507101CD, 0xF6AD, 0x46C8, 0x8E, 0x20, 0xEE, 0xB9, 0xE6, 0xBA, 0xC4, 0x7F); // 10547 "507101CD-F6AD-46C8-8E20-EEB9E6BAC47F"

enum CategoryType
{
    TYPE_PINNED,
    TYPE_RECENT,
    TYPE_FREQUENT,
};

struct APPDESTCATEGORY
{
    CategoryType type;
    union
    {
        LPWSTR name;
        int subType;
    };
    UINT count;
    UINT unknown;
};

// IInternalCustomDestinationList
DECLARE_INTERFACE_(IDestinationList, IUnknown)
{
public:
    STDMETHOD(SetMinItems)(UINT) PURE;
    STDMETHOD(SetApplicationID)(LPCWSTR appUserModelId) PURE;
    STDMETHOD(GetSlotCount)(UINT*) PURE;
    STDMETHOD(GetCategoryCount)(UINT* pCount) PURE;
    STDMETHOD(GetCategory)(UINT index, int getCatFlags, APPDESTCATEGORY* pCategory) PURE;
    STDMETHOD(DeleteCategory)(UINT index, UINT) PURE;
    STDMETHOD(EnumerateCategoryDestinations)(UINT index, REFIID riid, void** ppvObject) PURE;
    STDMETHOD(RemoveDestination)(IUnknown* pItem) PURE;
    STDMETHOD(ResolveDestination)(HWND hWnd, UINT p1, IShellItem* pShellItem, REFIID riid, void**ppvObject) PURE;
    STDMETHOD(Proc12)(UINT* p0, UINT* p1) PURE;
    STDMETHOD(Proc13)() PURE;
};

DEFINE_GUID(CLSID_AutomaticDestinationList, 0xF0AE1542, 0xF497, 0x484B, 0xA1, 0x75, 0xA2, 0x0D, 0xB0, 0x91, 0x44, 0xBA);

DECLARE_INTERFACE_IID_(IAutomaticDestinationList, IUnknown, "BC10DCE3-62F2-4BC6-AF37-DB46ED7873C4")
{
public:
    STDMETHOD(Initialize)(LPCWSTR appUserModelId, LPCWSTR lnkPath, LPCWSTR) PURE;
    STDMETHOD(HasList)(BOOL* pHasList) PURE;
    STDMETHOD(GetList)(CategoryType listType, UINT maxCount, REFIID riid, void** ppvObject) PURE;
    STDMETHOD(AddUsagePoint)(IUnknown* pItem) PURE;
    STDMETHOD(PinItem)(IUnknown* pItem, UINT pinIndex) PURE; // -1 - pin, -2 - unpin
    STDMETHOD(IsPinned)(IUnknown* pItem, UINT* pinIndex) PURE;
    STDMETHOD(RemoveDestination)(IUnknown* pItem) PURE;
    STDMETHOD(SetUsageData)() PURE;
    STDMETHOD(GetUsageData)() PURE;
    STDMETHOD(ResolveDestination)(HWND hWnd, UINT p1, IShellItem* pShellItem, REFIID riid, void** ppvObject) PURE;
    STDMETHOD(ClearList)(CategoryType listType) PURE;
};

DECLARE_INTERFACE_IID_(IAutomaticDestinationList10b, IUnknown, "E9C5EF8D-FD41-4F72-BA87-EB03BAD5817C") // 10547
{
public:
    STDMETHOD(Initialize)(LPCWSTR appUserModelId, LPCWSTR lnkPath, LPCWSTR) PURE;
    STDMETHOD(HasList)(BOOL* pHasList) PURE;
    STDMETHOD(GetList)(CategoryType listType, UINT maxCount, UINT flags, REFIID riid, void** ppvObject) PURE;
    STDMETHOD(AddUsagePoint)(IUnknown * pItem) PURE;
    STDMETHOD(PinItem)(IUnknown* pItem, UINT pinIndex) PURE; // -1 - pin, -2 - unpin
    STDMETHOD(IsPinned)(IUnknown* pItem, UINT* pinIndex) PURE;
    STDMETHOD(RemoveDestination)(IUnknown* pItem) PURE;
    STDMETHOD(SetUsageData)() PURE;
    STDMETHOD(GetUsageData)() PURE;
    STDMETHOD(ResolveDestination)(HWND hWnd, UINT p1, IShellItem* pShellItem, REFIID riid, void** ppvObject) PURE;
    STDMETHOD(ClearList)(CategoryType listType) PURE;
};

///////////////////////////////////////////////////////////////////////////////

// See https://learn.microsoft.com/en-us/previous-versions/windows/apps/hh965372(v=win.10)#scale
enum RESOURCE_SCALE
{
    RES_SCALE_100 = 0,
    RES_SCALE_140 = 1,
    RES_SCALE_180 = 2,
    RES_SCALE_80 = 3,
};

// See https://learn.microsoft.com/en-us/previous-versions/windows/apps/hh965372(v=win.10)#contrast
enum RESOURCE_CONTRAST
{
    RES_CONTRAST_STANDARD = 0,
    RES_CONTRAST_HIGH = 1,
    RES_CONTRAST_BLACK = 2,
    RES_CONTRAST_WHITE = 3,
};

DECLARE_INTERFACE_IID_(IResourceContext, IUnknown, "E3C22B30-8502-4B2F-9133-559674587E51")
{
    STDMETHOD(GetLanguage)(LPWSTR* pLanguage) PURE;
    STDMETHOD(GetHomeRegion)(LPWSTR* pRegion) PURE;
    STDMETHOD(GetLayoutDirection)(enum RESOURCE_LAYOUT_DIRECTION* pDirection) PURE;
    STDMETHOD(GetTargetSize)(WORD* pSize) PURE;
    STDMETHOD(GetScale)(RESOURCE_SCALE* pScale) PURE;
    STDMETHOD(GetContrast)(RESOURCE_CONTRAST* pContrast) PURE;
    STDMETHOD(GetAlternateForm)(LPWSTR* pForm) PURE;
    STDMETHOD(GetQualifierValue)(LPCWSTR name, LPWSTR* pValue) PURE;
    STDMETHOD(SetLanguage)(LPCWSTR language) PURE;
    STDMETHOD(SetHomeRegion)(LPCWSTR region) PURE;
    STDMETHOD(SetLayoutDirection)(enum RESOURCE_LAYOUT_DIRECTION direction) PURE;
    STDMETHOD(SetTargetSize)(WORD size) PURE;
    STDMETHOD(SetScale)(RESOURCE_SCALE scale) PURE;
    STDMETHOD(SetContrast)(RESOURCE_CONTRAST contrast) PURE;
    STDMETHOD(SetAlternateForm)(LPCWSTR form) PURE;
};

///////////////////////////////////////////////////////////////////////////////

DECLARE_INTERFACE_IID_(IResourceMap, IUnknown, "6E21E72B-B9B0-42AE-A686-983CF784EDCD")
{
    STDMETHOD(GetUri)(LPCWSTR* pUri) PURE;
    STDMETHOD(GetSubtree)(LPCWSTR propName, IResourceMap** pSubTree) PURE;
    STDMETHOD(GetString)(LPCWSTR propName, LPWSTR* pString) PURE;
    STDMETHOD(GetStringForContext)(IResourceContext* pContext, LPCWSTR propName, LPWSTR* pString) PURE;
    STDMETHOD(GetFilePath)(LPCWSTR propName, LPWSTR* pPath) PURE;
    STDMETHOD(GetFilePathForContext)(IResourceContext* pContext, LPCWSTR propName, LPWSTR* pPath) PURE;
};

///////////////////////////////////////////////////////////////////////////////

DEFINE_GUID(CLSID_ResourceManager, 0xDBCE7E40, 0x7345, 0x439D, 0xB1, 0x2C, 0x11, 0x4A, 0x11, 0x81, 0x9A, 0x09);

// IMrtResourceManager
DECLARE_INTERFACE_IID_(IResourceManager, IUnknown, "130A2F65-2BE7-4309-9A58-A9052FF2B61C")
{
public:
    STDMETHOD(Initialize)(void) PURE;
    STDMETHOD(InitializeForCurrentApplication)(void) PURE;
    STDMETHOD(InitializeForPackage)(LPCWSTR name) PURE;
    STDMETHOD(InitializeForFile)(LPCWSTR fname) PURE;
    STDMETHOD(GetMainResourceMap)(REFIID riid, void** ppvObject) PURE;
    STDMETHOD(GetResourceMap)(LPCWSTR name, REFIID riid, void** ppvObject) PURE;
    STDMETHOD(GetDefaultContext)(REFIID riid, void** ppvObject) PURE;
    STDMETHOD(GetReference)(REFIID riid, void** ppvObject) PURE;
    STDMETHOD(Proc11)(LPCWSTR p0, UINT* p1) PURE;
};

///////////////////////////////////////////////////////////////////////////////

//  Name:     System.AppUserModel.HostEnvironment -- PKEY_AppUserModel_HostEnvironment
//  Type:     UInt32 -- VT_UI4
//  FormatID: {9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3}, 14
DEFINE_PROPERTYKEY(PKEY_AppUserModel_HostEnvironment, 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3, 14);

//  Name:     System.AppUserModel.ActivationContext -- PKEY_AppUserModel_ActivationContext
//  Type:     String -- VT_LPWSTR
//  FormatID: {9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3}, 20
DEFINE_PROPERTYKEY(PKEY_AppUserModel_ActivationContext, 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3, 20);

//  Type:     String -- VT_LPWSTR
//  FormatID: {9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3}, 29
DEFINE_PROPERTYKEY(PKEY_AppUserModel_DestListLogoUri, 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3, 29);

DEFINE_PROPERTYKEY(PKEY_MetroPackageName, 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3, 21);
//DEFINE_PROPERTYKEY(PKEY_MetroIcon, 0x86D40B4D, 0x9069, 0x443C, 0x81, 0x9A, 0x2A, 0x54, 0x09, 0x0D, 0xCC, 0xEC, 2);
