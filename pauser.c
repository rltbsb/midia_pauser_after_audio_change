
#define COBJMACROS
#include <windows.h>
#include <mmdeviceapi.h>
#include <shellapi.h>

#include <strsafe.h>
#include "resource.h"

static const CLSID CLSID_MMDeviceEnumerator = {0xbcde0395,0xe52f,0x467c,{0x8e,0x3d,0xc4,0x57,0x92,0x91,0x69,0x2e}};
static const IID IID_IMMDeviceEnumerator = {0xa95664d2,0x9614,0x4f35,{0xa7,0x46,0xde,0x8d,0xb6,0x36,0x17,0xe6}};
static const IID IID_IMMNotificationClient = {0x7991eec9,0x7e89,0x4d85,{0x83,0x90,0x6c,0x70,0x3e,0x8b,0x8e,0x78}};

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")

#define WM_TRAYICON (WM_APP + 1)
#define ID_TOGGLE   1001
#define ID_EXIT     1002

static BOOL g_active = TRUE;
static NOTIFYICONDATA g_nid = {0};
static HICON g_iconOn = NULL;
static HICON g_iconOff = NULL;
static HICON g_iconApp = NULL;
static IMMDeviceEnumerator *g_enumerator = NULL;

/* Forward declarations */
static void ToggleActive(void);
static void ShowContextMenu(HWND hwnd);
static void SendPauseCommand(void);

/* Create on/off toggle icons similar to the provided images */
/* Notification client implementation */
typedef struct {
    IMMNotificationClientVtbl *lpVtbl;
    LONG ref;
} DeviceNotificationClient;

/* Forward declarations for COM reference counting functions */
ULONG STDMETHODCALLTYPE DNC_AddRef(IMMNotificationClient *This);
ULONG STDMETHODCALLTYPE DNC_Release(IMMNotificationClient *This);

HRESULT STDMETHODCALLTYPE DNC_QueryInterface(IMMNotificationClient *This, REFIID riid, void **ppv)
{
    if (ppv == NULL) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IMMNotificationClient)) {
        *ppv = This;
        DNC_AddRef(This);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DNC_AddRef(IMMNotificationClient *This)
{
    DeviceNotificationClient *self = (DeviceNotificationClient*)This;
    return InterlockedIncrement(&self->ref);
}

ULONG STDMETHODCALLTYPE DNC_Release(IMMNotificationClient *This)
{
    DeviceNotificationClient *self = (DeviceNotificationClient*)This;
    LONG ref = InterlockedDecrement(&self->ref);
    if (ref == 0) {
        HeapFree(GetProcessHeap(),0,self);
    }
    return ref;
}

HRESULT STDMETHODCALLTYPE DNC_OnDeviceStateChanged(IMMNotificationClient *This, LPCWSTR pwstrDeviceId, DWORD dwNewState)
{ return S_OK; }
HRESULT STDMETHODCALLTYPE DNC_OnDeviceAdded(IMMNotificationClient *This, LPCWSTR pwstrDeviceId)
{ return S_OK; }
HRESULT STDMETHODCALLTYPE DNC_OnDeviceRemoved(IMMNotificationClient *This, LPCWSTR pwstrDeviceId)
{ return S_OK; }

HRESULT STDMETHODCALLTYPE DNC_OnDefaultDeviceChanged(IMMNotificationClient *This, EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
{
    if (flow == eRender && role == eConsole && g_active) {
        SendPauseCommand();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DNC_OnPropertyValueChanged(IMMNotificationClient *This, LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{ return S_OK; }

static IMMNotificationClientVtbl g_DNC_Vtbl = {
    DNC_QueryInterface,
    DNC_AddRef,
    DNC_Release,
    DNC_OnDeviceStateChanged,
    DNC_OnDeviceAdded,
    DNC_OnDeviceRemoved,
    DNC_OnDefaultDeviceChanged,
    DNC_OnPropertyValueChanged
};

static DeviceNotificationClient* CreateNotificationClient(void)
{
    DeviceNotificationClient *client = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DeviceNotificationClient));
    if (client) {
        client->lpVtbl = &g_DNC_Vtbl;
        client->ref = 1;
    }
    return client;
}

static DeviceNotificationClient *g_notify = NULL;

static void SendPauseCommand(void)
{
    SendMessageW(HWND_BROADCAST, WM_APPCOMMAND, 0, MAKELPARAM(0, APPCOMMAND_MEDIA_PAUSE));
}

static void ToggleActive(void)
{
    g_active = !g_active;
    g_nid.hIcon = g_active ? g_iconOn : g_iconOff;
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
}

static void ShowContextMenu(HWND hwnd)
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TOGGLE, g_active ? TEXT("Off") : TEXT("On"));
    AppendMenu(hMenu, MF_STRING, ID_EXIT, TEXT("Exit"));
    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN|TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        g_nid.cbSize = sizeof(NOTIFYICONDATA);
        g_nid.hWnd = hwnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = g_iconOn;
        StringCchCopy(g_nid.szTip, ARRAYSIZE(g_nid.szTip), TEXT("Audio device watcher"));
        Shell_NotifyIcon(NIM_ADD, &g_nid);
        break;
    case WM_TRAYICON:
        if (lParam == WM_LBUTTONUP) {
            ToggleActive();
        } else if (lParam == WM_RBUTTONUP) {
            ShowContextMenu(hwnd);
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TOGGLE: ToggleActive(); break;
        case ID_EXIT: DestroyWindow(hwnd); break;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        Shell_NotifyIcon(NIM_DELETE, &g_nid);
        if (g_iconOn) DestroyIcon(g_iconOn);
        if (g_iconOff) DestroyIcon(g_iconOff);
        if (g_iconApp) DestroyIcon(g_iconApp);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nShowCmd)
{
    HANDLE hMutex = CreateMutex(NULL, TRUE, TEXT("midia_pauser_instance"));
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    CoInitialize(NULL);
    if (FAILED(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                                &IID_IMMDeviceEnumerator, (void**)&g_enumerator))) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }
    g_notify = CreateNotificationClient();
    if (g_notify) {
        IMMDeviceEnumerator_RegisterEndpointNotificationCallback(g_enumerator, (IMMNotificationClient*)g_notify);
    }

    int smcx = GetSystemMetrics(SM_CXSMICON);
    int smcy = GetSystemMetrics(SM_CYSMICON);
    g_iconOn = LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON_ON), IMAGE_ICON, smcx, smcy, 0);
    g_iconOff = LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON_OFF), IMAGE_ICON, smcx, smcy, 0);
    g_iconApp = LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON_ON), IMAGE_ICON,
                          GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("AudioWatcherClass");
    wc.hIcon = g_iconApp;
    wc.hIconSm = g_iconOn;
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, TEXT("AudioWatcher"), 0, 0,0,0,0, NULL, NULL, hInstance, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (g_notify) {
        IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(g_enumerator, (IMMNotificationClient*)g_notify);
        DNC_Release((IMMNotificationClient*)g_notify);
    }
    if (g_enumerator) IMMDeviceEnumerator_Release(g_enumerator);
    CoUninitialize();
    if (hMutex) CloseHandle(hMutex);
    return 0;
}

