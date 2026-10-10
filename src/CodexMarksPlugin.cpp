#include <windows.h>
#include <comdef.h>
#include <string>

#import "VGCoreAuto.tlb" rename_namespace("VGCore") named_guids

using namespace VGCore;

static const wchar_t* kAddMarks = L"CodexMarks.AddMarkPoints";
static const wchar_t* kCutGrid = L"CodexMarks.CreateCutGrid";
static const wchar_t* kCutPlot = L"CodexMarks.CutPlotOutput";

static HINSTANCE g_hInst = nullptr;
static _ApplicationPtr g_app;
static DWORD g_cookie = 0;

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

class AppEvents : public IDispatch
{
    LONG refs_;

public:
    AppEvents() : refs_(1) {}

    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_IDispatch || riid == __uuidof(DIVGApplicationEvents))
        {
            *ppv = static_cast<IDispatch*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() override { return InterlockedIncrement(&refs_); }
    STDMETHOD_(ULONG, Release)() override
    {
        ULONG r = InterlockedDecrement(&refs_);
        if (!r) delete this;
        return r;
    }

    STDMETHOD(GetTypeInfoCount)(UINT* pctinfo) override
    {
        if (pctinfo) *pctinfo = 0;
        return S_OK;
    }

    STDMETHOD(GetTypeInfo)(UINT, LCID, ITypeInfo**) override { return E_NOTIMPL; }
    STDMETHOD(GetIDsOfNames)(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override { return E_NOTIMPL; }

    STDMETHOD(Invoke)(DISPID dispid, REFIID, LCID, WORD, DISPPARAMS* params, VARIANT* result, EXCEPINFO*, UINT*) override
    {
        if (dispid == 0x0015 && params && params->cArgs >= 1)
        {
            VARIANTARG& arg = params->rgvarg[0];
            if (arg.vt == VT_BSTR)
            {
                std::wstring cmd(arg.bstrVal, SysStringLen(arg.bstrVal));
                if (_wcsicmp(cmd.c_str(), kAddMarks) == 0) RunScript(L"CDR_AddMarkPoints.vbs");
                else if (_wcsicmp(cmd.c_str(), kCutGrid) == 0) RunScript(L"CDR_CreateCutGrid.vbs");
                else if (_wcsicmp(cmd.c_str(), kCutPlot) == 0) RunScript(L"CDR_CutPlotOutput.vbs");
            }
        }
        else if (dispid == 0x0016 && params && params->cArgs >= 3)
        {
            VARIANTARG& enabled = params->rgvarg[1];
            VARIANTARG& check = params->rgvarg[0];
            if ((enabled.vt & VT_BYREF) && enabled.pboolVal) *enabled.pboolVal = VARIANT_TRUE;
            if ((check.vt & VT_BYREF) && check.plVal) *check.plVal = 0;
        }
        if (result) VariantInit(result);
        return S_OK;
    }
};

static AppEvents* g_events = nullptr;

extern "C" __declspec(dllexport) DWORD APIENTRY AttachPlugin(IDispatch* app)
{
    if (!app) return 0;
    try
    {
        g_app = app;
        g_app->AddPluginCommand(_bstr_t(kAddMarks), _bstr_t(L"添加标记点"), _bstr_t(L"添加圆形/直角定位标记点"));
        g_app->AddPluginCommand(_bstr_t(kCutGrid), _bstr_t(L"创建切割表格"), _bstr_t(L"按行列创建切割表格"));
        g_app->AddPluginCommand(_bstr_t(kCutPlot), _bstr_t(L"刻绘输出"), _bstr_t(L"输出刻绘文件"));
        g_events = new AppEvents();
        g_cookie = g_app->AdviseEvents(static_cast<IDispatch*>(g_events));
    }
    catch (...)
    {
        return 0;
    }
    return 1;
}

extern "C" __declspec(dllexport) void APIENTRY DetachPlugin()
{
    try
    {
        if (g_app && g_cookie) g_app->UnadviseEvents(g_cookie);
        if (g_events) g_events->Release();
        g_events = nullptr;
        g_cookie = 0;
        g_app = nullptr;
    }
    catch (...) {}
}

BOOL APIENTRY DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) g_hInst = hinst;
    return TRUE;
}
