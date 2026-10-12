#include <windows.h>
#include <comdef.h>
#include <string>

#import "VGCoreAuto.tlb" rename_namespace("VGCore") named_guids \
    rename("GetCommandLine", "VGGetCommandLine") \
    rename("CopyFile", "VGCopyFile") \
    rename("FindWindow", "VGFindWindow")

using namespace VGCore;

static const wchar_t* kAddMarks = L"CodexMarks.AddMarkPoints";
static const wchar_t* kCutGrid = L"CodexMarks.CreateCutGrid";
static const wchar_t* kCutPlot = L"CodexMarks.CutPlotOutput";

static HINSTANCE g_hInst = nullptr;

static std::wstring GetScriptDir()
{
    wchar_t appData[MAX_PATH] = {0};
    ExpandEnvironmentStringsW(L"%APPDATA%", appData, MAX_PATH);
    return std::wstring(appData) + L"\\Corel\\CorelDRAW Graphics Suite 2022\\Draw\\Scripts\\CodexMarksCutGrid";
}

static void RunScript(const wchar_t* name)
{
    std::wstring script = GetScriptDir() + L"\\" + name;
    std::wstring args = L"\"" + script + L"\"";
    ShellExecuteW(nullptr, L"open", L"wscript.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
}

class CodexMarksPlugin final : public IVGAppPlugin
{
    LONG refs_ = 1;
    IVGApplication* app_ = nullptr;
    DWORD eventCookie_ = 0;

public:
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) override
    {
        if (!ppvObject) return E_POINTER;
        *ppvObject = nullptr;

        if (riid == IID_IUnknown || riid == IID_IDispatch || riid == __uuidof(IVGAppPlugin))
        {
            *ppvObject = static_cast<IVGAppPlugin*>(this);
            AddRef();
            return S_OK;
        }

        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() override
    {
        return InterlockedIncrement(&refs_);
    }

    STDMETHOD_(ULONG, Release)() override
    {
        ULONG refs = InterlockedDecrement(&refs_);
        if (!refs) delete this;
        return refs;
    }

    STDMETHOD(GetTypeInfoCount)(UINT* pctinfo) override
    {
        if (pctinfo) *pctinfo = 0;
        return S_OK;
    }

    STDMETHOD(GetTypeInfo)(UINT, LCID, ITypeInfo**) override
    {
        return E_NOTIMPL;
    }

    STDMETHOD(GetIDsOfNames)(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override
    {
        return E_NOTIMPL;
    }

    STDMETHOD(Invoke)(DISPID dispidMember, REFIID, LCID, WORD, DISPPARAMS* params, VARIANT* result, EXCEPINFO*, UINT*) override
    {
        if (result) VariantInit(result);

        if (dispidMember == 0x0014 && params && params->cArgs >= 1)
        {
            VARIANTARG& arg = params->rgvarg[0];
            if (arg.vt == VT_BSTR && arg.bstrVal)
            {
                std::wstring command(arg.bstrVal, SysStringLen(arg.bstrVal));
                if (_wcsicmp(command.c_str(), kAddMarks) == 0) RunScript(L"CDR_AddMarkPoints.vbs");
                else if (_wcsicmp(command.c_str(), kCutGrid) == 0) RunScript(L"CDR_CreateCutGrid.vbs");
                else if (_wcsicmp(command.c_str(), kCutPlot) == 0) RunScript(L"CDR_CutPlotOutput.vbs");
            }
        }
        else if (dispidMember == 0x0015 && params && params->cArgs >= 3)
        {
            VARIANTARG& enabled = params->rgvarg[1];
            VARIANTARG& checked = params->rgvarg[0];
            if ((enabled.vt & VT_BYREF) && enabled.pboolVal) *enabled.pboolVal = VARIANT_TRUE;
            if ((checked.vt & VT_BYREF) && checked.plVal) *checked.plVal = 0;
        }

        return S_OK;
    }

    STDMETHOD(raw_OnLoad)(IVGApplication* Application) override
    {
        app_ = Application;
        if (app_) app_->AddRef();
        return S_OK;
    }

    STDMETHOD(raw_StartSession)() override
    {
        if (!app_) return S_OK;

        try
        {
            app_->AddPluginCommand(_bstr_t(kAddMarks), _bstr_t(L"添加标记点"), _bstr_t(L"添加圆形/直角定位标记点"));
            app_->AddPluginCommand(_bstr_t(kCutGrid), _bstr_t(L"创建切割表格"), _bstr_t(L"按行列创建切割表格"));
            app_->AddPluginCommand(_bstr_t(kCutPlot), _bstr_t(L"刻绘输出"), _bstr_t(L"输出刻绘文件"));
            app_->AdviseEvents(this, &eventCookie_);
        }
        catch (...) {}

        return S_OK;
    }

    STDMETHOD(raw_StopSession)() override
    {
        try
        {
            if (app_ && eventCookie_)
            {
                app_->UnadviseEvents(eventCookie_);
                eventCookie_ = 0;
            }
        }
        catch (...) {}

        return S_OK;
    }

    STDMETHOD(raw_OnUnload)() override
    {
        raw_StopSession();
        if (app_)
        {
            app_->Release();
            app_ = nullptr;
        }
        return S_OK;
    }
};

extern "C" __declspec(dllexport) DWORD APIENTRY AttachPlugin(IVGAppPlugin** plugin)
{
    if (!plugin) return 0;
    *plugin = new CodexMarksPlugin();
    return 0x100;
}

BOOL APIENTRY DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hInst = hinst;
        DisableThreadLibraryCalls(hinst);
    }
    return TRUE;
}
