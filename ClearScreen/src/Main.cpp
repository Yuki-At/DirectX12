#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

using namespace Microsoft::WRL;

#define WIDTH (640)
#define HEIGHT (480)

#define ReturnIfFailed(result) if (FAILED(result)) {\
            TCHAR buffer[256];\
            wsprintf(buffer, TEXT("!ERROR!\n%s:[%d]\n"), __FUNCTIONW__, __LINE__);\
            OutputDebugString(buffer);\
            return result;\
        }

// Win32 objects.
HINSTANCE hInstance;
HWND hWindow;

// Pipeline objects.
ComPtr<ID3D12Device> device;
ComPtr<ID3D12CommandQueue> commandQueue;
ComPtr<IDXGISwapChain> swapChain;

HRESULT InitWindow();
HRESULT LoadPipeline();
HRESULT LoadAssets();
void OnUpdate();
void OnRender();
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow) {
    MSG msg = { };

    ::hInstance = hInstance;

    if (FAILED(InitWindow())) {
        return -10;
    }

    if (FAILED(LoadPipeline())) {
        return -11;
    }

    if (FAILED(LoadAssets())) {
        return -12;
    }

    ShowWindow(hWindow, nCmdShow);

    while (GetMessage(&msg, nullptr, 0, 0)) {
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

HRESULT InitWindow() {
    LPCTSTR WindowClassName = TEXT("WindowClass");
    WNDCLASS wc;

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProcedure;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = WindowClassName;

    if (!RegisterClass(&wc)) {
        return E_FAIL;
    }

    hWindow = CreateWindow(
        WindowClassName, TEXT("ClearScreen - DirectX12"),
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hWindow) {
        return E_FAIL;
    }

    // Resize Window.
    {
        RECT rd, rw, rc;
        int width, height;

        GetWindowRect(GetDesktopWindow(), &rd);
        GetWindowRect(hWindow, &rw);
        GetClientRect(hWindow, &rc);

        width = (rw.right - rw.left) - (rc.right - rc.left) + WIDTH;
        height = (rw.bottom - rw.top) - (rc.bottom - rc.top) + HEIGHT;

        SetWindowPos(hWindow, HWND_TOP, (rd.right - width) / 2, (rd.bottom - height) / 2, width, height, 0);
    }

    return S_OK;
}

HRESULT LoadPipeline() {
    ComPtr<IDXGIFactory4> factory4;
    ReturnIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory4)));

    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(factory4->QueryInterface(IID_PPV_ARGS(&factory6)))) {
        factory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
    } else {
        factory4->EnumAdapters1(0, &adapter);
    }

    ReturnIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

    // Command Queue
    {
        D3D12_COMMAND_QUEUE_DESC desc;
        desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;
        ReturnIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)));
    }

    // Swap Chain
    {
        DXGI_SWAP_CHAIN_DESC1 desc;

    }

    return S_OK;
}

HRESULT LoadAssets() {
    return S_OK;
}

void OnUpdate() { }

void OnRender() { }

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        OnUpdate();
        OnRender();
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
