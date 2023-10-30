#define STRICT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "ShellUtils.h"

#include <ShellApi.h>
#include <ShObjIdl.h>
#include <ShlObj.h>
#include <propvarutil.h>

#include "Rad/Log.h"
#include "Rad/MemoryPlus.h"
#include "Rad/Format.h"

void DumpPropertyStore(IPropertyStore* pStore)
{
    DWORD count = 0;
    CHECK_HR(pStore->GetCount(&count));
    for (DWORD i = 0; i < count; ++i)
    {
        PROPERTYKEY key = {};
        CHECK_HR(pStore->GetAt(i, &key));

        TCHAR strkey[1024];
        CHECK_HR(PSStringFromPropertyKey(key, strkey, ARRAYSIZE(strkey)));

        PROPVARIANT val = {};
        PropVariantInit(&val);
        CHECK_HR(pStore->GetValue(key, &val));

        TCHAR strval[1024];
        PropVariantToString(val, strval, ARRAYSIZE(strval));

        OutputDebugString(Format(TEXT("%s = %s\n"), strkey, strval).c_str());

        CHECK_HR(PropVariantClear(&val));
    }
}

CString GetPropertyStoreString(IPropertyStore* pStore, REFPROPERTYKEY key)
{
    PROPVARIANT val;
    PropVariantInit(&val);
    CString res;
    if (SUCCEEDED(pStore->GetValue(key, &val)))
    {
        switch (val.vt)
        {
        case VT_EMPTY:
            break;

        case VT_LPWSTR: case VT_BSTR:
            res = val.pwszVal;
            break;

        case VT_LPSTR:
            res = val.pszVal;
            break;

        default:
            ATLASSERT(FALSE);
            break;
        }
    }
    PropVariantClear(&val);
    return res;
}

BOOL GetPropertyStoreBool(IPropertyStore* pStore, REFPROPERTYKEY key)
{
    PROPVARIANT val;
    PropVariantInit(&val);
    BOOL res = FALSE;
    if (SUCCEEDED(pStore->GetValue(key, &val)))
    {
        switch (val.vt)
        {
        case VT_EMPTY:
            res = FALSE;
            break;

        case VT_BOOL:
            res = val.boolVal != 0;
            break;

        default:
            ATLASSERT(FALSE);
            break;
        }
    }
    PropVariantClear(&val);
    return res;
}

CString LoadIndirectString(_In_ PCWSTR pszSource)
{
    CString str;
    //CHECK_HR(SHLoadIndirectString(pszSource, str.GetBufferSetLength(1024), 1024, nullptr));
    if (FAILED(SHLoadIndirectString(pszSource, str.GetBufferSetLength(1024), 1024, nullptr)))
    {
        str.ReleaseBuffer();
        return pszSource;
    }
    else
        str.ReleaseBuffer();
    return str;
}

void OpenInExplorer(IShellFolder* pFolder)
{
    CComHeapPtr<ITEMIDLIST_ABSOLUTE> spidl;
    CHECK_HR(SHGetIDListFromObject(pFolder, &spidl));

    CComPtr<IWebBrowser2> pwb;
    CHECK_HR(pwb.CoCreateInstance(CLSID_ShellBrowserWindow, nullptr, CLSCTX_LOCAL_SERVER));

    CHECK_HR(CoAllowSetForegroundWindow(pwb, 0));
    CHECK_RET(pwb, void());

    CComVariant varTarget;
    CHECK_HR(InitVariantFromBuffer(spidl, ILGetSize(spidl), &varTarget));

    CComVariant vEmpty;
    CHECK_HR(pwb->Navigate2(&varTarget, &vEmpty, &vEmpty, &vEmpty, &vEmpty));
    CHECK_HR(pwb->put_Visible(VARIANT_TRUE));
}

void OpenDefaultItem(HWND const hWnd, IContextMenu* const pContextMenu, const CString& params)
{
    auto hMenu = MakeUniqueHandle(CreatePopupMenu(), DestroyMenu);
    CHECK_HR(pContextMenu->QueryContextMenu(hMenu.get(), 0, 1, 1000, CMF_DEFAULTONLY));
    const int id = GetMenuDefaultItem(hMenu.get(), FALSE, 0);

    if (id >= 0)
    {
        CStringA paramsa;
        paramsa = CT2CA(params);

        CMINVOKECOMMANDINFO cmd = {};
        cmd.cbSize = sizeof(cmd);
        cmd.hwnd = hWnd;
        cmd.fMask = CMIC_MASK_FLAG_LOG_USAGE | CMIC_MASK_ASYNCOK;
        cmd.lpVerb = MAKEINTRESOURCEA(id - 1);
        if (!params.IsEmpty())
            cmd.lpParameters = paramsa;
        cmd.nShow = SW_SHOWNORMAL;
        CHECK_HR(pContextMenu->InvokeCommand(&cmd));
    }
}

CString CStrRet::toStr(_In_opt_ PCUITEMID_CHILD pidl) const
{
    CString s;
    CHECK_HR(StrRetToBuf(const_cast<CStrRet*>(this), pidl, s.GetBufferSetLength(1024), 1024));
    s.ReleaseBuffer();
    return s;
}

int GetIconIndex(IShellItem* pShellItem)
{
    CComQIPtr<IParentAndItem> pParentAndItem(pShellItem);

    CComPtr<IShellFolder> pFolder;
    CComHeapPtr<ITEMIDLIST> pIdList;
    pParentAndItem->GetParentAndItem(nullptr, &pFolder, &pIdList);

    CComQIPtr<IShellIcon> pShellIcon(pFolder);
    int nIconIndex = 0;
    CHECK_HR(pShellIcon->GetIconOf(pIdList, GIL_FORSHELL, &nIconIndex));
    return nIconIndex;
}
