#pragma once

#include <atlstr.h>
#include <propkeydef.h>

interface IPropertyStore;
interface IShellFolder;
interface IShellItem;
interface IContextMenu;

void DumpPropertyStore(IPropertyStore* pStore);
CString GetPropertyStoreString(IPropertyStore* pStore, REFPROPERTYKEY key);
BOOL GetPropertyStoreBool(IPropertyStore* pStore, REFPROPERTYKEY key);

CString LoadIndirectString(_In_ PCWSTR pszSource);

void OpenInExplorer(IShellFolder* pFolder);
void OpenDefaultItem(HWND const hWnd, IContextMenu* const pContextMenu, const CString& params = CString());

class CStrRet : public STRRET
{
public:
    ~CStrRet()
    {
        if (uType == STRRET_WSTR)
            CoTaskMemFree(pOleStr);
    }

    CString toStr(_In_opt_ PCUITEMID_CHILD pidl) const;
};

int GetIconIndex(IShellItem* pShellItem);
