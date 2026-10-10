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
static IDispatch* g_app = nullptr;
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

static HRESULT InvokeDispatch(IDispatch* target, const wchar_t* method, VARIANT* args, UINT argc, VARIANT* result = nullptr)
{
    if (!target) return E_POINTER;
    DISPID dispid = 0;
    LPOLESTR name = const_cast<LPOLESTR>(method);
    HRESULT hr = target->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) return hr;

    DISPPARAMS params = {};
    params.rgvarg = args;
    params.cArgs = argc;
    return target->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, result, nullptr, nullptr);
}

static void AddPluginCommand(IDispatch* app, const wchar_t* id, const wchar_t* caption, const wchar_t* tooltip)
{
    VARIANT args[3];
    for (auto& arg : args) VariantInit(&arg);
    args[2].vt = VT_BSTR; args[2].bstrVal = SysAllocString(id);
    args[1].vt = VT_BSTR; args[1].bstrVal = SysAllocString(caption);
    args[0].vt = VT_BSTR; args[0].bstrVal = SysAllocString(tooltip);
    InvokeDispatch(app, L"AddPluginCommand", args, 3);
    for (auto& arg : args) VariantClear(&arg);
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
        g_app->AddRef();
        AddPluginCommand(g_app, kAddMarks, L"添加标记点", L"添加圆形/直角定位标记点");
        AddPluginCommand(g_app, kCutGrid, L"创建切割表格", L"按行列创建切割表格");
        AddPluginCommand(g_app, kCutPlot, L"刻绘输出", L"输出刻绘文件");

        g_events = new AppEvents();
        VARIANT args[1];
        VariantInit(&args[0]);
        args[0].vt = VT_DISPATCH;
        args[0].pdispVal = static_cast<IDispatch*>(g_events);
        VARIANT result;
        VariantInit(&result);
        if (SUCCEEDED(InvokeDispatch(g_app, L"AdviseEvents", args, 1, &result)))
        {
            if (result.vt == VT_I4 || result.vt == VT_INT) g_cookie = static_cast<DWORD>(result.lVal);
        }
        VariantClear(&result);
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
        if (g_app && g_cookie)
        {
            VARIANT args[1];
            VariantInit(&args[0]);
            args[0].vt = VT_I4;
            args[0].lVal = static_cast<LONG>(g_cookie);
            InvokeDispatch(g_app, L"UnadviseEvents", args, 1);
        }
        if (g_events) g_events->Release();
        g_events = nullptr;
        g_cookie = 0;
        if (g_app) g_app->Release();
        g_app = nullptr;
    }
    catch (...) {}
}

BOOL APIENTRY DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) g_hInst = hinst;
    return TRUE;
}
