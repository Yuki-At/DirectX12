#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

using namespace Microsoft::WRL;

#define ReturnIfFailed(result) if (FAILED(result)) {\
            TCHAR buffer[256];\
            wsprintf(buffer, TEXT("!ERROR!\n%s:[%d]\n"), __FUNCTIONW__, __LINE__);\
            OutputDebugString(buffer);\
            return result;\
        }

constexpr UINT Width = 640;
constexpr UINT Height = 480;
constexpr UINT FrameCount = 2;

// Win32 objects.
HINSTANCE hInstance;
HWND hWindow;

// Pipeline objects.
ComPtr<ID3D12Device> device;
ComPtr<ID3D12CommandQueue> commandQueue;
ComPtr<IDXGISwapChain4> swapChain;
ComPtr<ID3D12DescriptorHeap> rtvHeap;
UINT rtvDescriptorSize;
ComPtr<ID3D12Resource> renderTargets[FrameCount];
ComPtr<ID3D12CommandAllocator> commandAllocator;
ComPtr<ID3D12GraphicsCommandList> commandList;

// Synchronization objects.
UINT frameIndex;

HRESULT InitWindow();
HRESULT InitDirectX();
HRESULT OnUpdate();
HRESULT OnRender();
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow) {
    MSG msg = { };

    ::hInstance = hInstance;

    if (FAILED(InitWindow())) {
        return -10;
    }

    if (FAILED(InitDirectX())) {
        return -11;
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

        width = (rw.right - rw.left) - (rc.right - rc.left) + Width;
        height = (rw.bottom - rw.top) - (rc.bottom - rc.top) + Height;

        SetWindowPos(hWindow, HWND_TOP, (rd.right - width) / 2, (rd.bottom - height) / 2, width, height, 0);
    }

    return S_OK;
}

HRESULT InitDirectX() {
    UINT factoryFlags = 0;

#if defined(DEBUG) | defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory4;
    ReturnIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory4)));

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
        desc.Width              = 0;
        desc.Height             = 0;
        desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Stereo             = false;
        desc.SampleDesc.Count   = 1;
        desc.SampleDesc.Quality = 0;
        desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount        = FrameCount;
        desc.Scaling            = DXGI_SCALING_STRETCH;
        desc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD; // DXGI_SWAP_EFFECT_DISCARD Ç∆ä‘à·Ç¶Ç»Ç¢ÇÊÇ§Ç…ÅIÅIÅI
        desc.AlphaMode          = DXGI_ALPHA_MODE_UNSPECIFIED;
        desc.Flags              = 0;

        ComPtr<IDXGISwapChain1> swapChain1;
        ReturnIfFailed(factory4->CreateSwapChainForHwnd(
            commandQueue.Get(),
            hWindow,
            &desc,
            nullptr,
            nullptr,
            &swapChain1
        ));

        ReturnIfFailed(swapChain1.As(&swapChain));
        frameIndex = swapChain->GetCurrentBackBufferIndex();
    }

    // Descriptor Heap for RTV
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc;
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = FrameCount;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask       = 0;

        ReturnIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap)));

        rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(desc.Type);
    }

    // Render Target View (RTV)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < FrameCount; i++) {
            ReturnIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])));
            device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);

            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    // Command Allocator
    ReturnIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));

    // Command List
    ReturnIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
    ReturnIfFailed(commandList->Close());

    return S_OK;
}

HRESULT OnUpdate() {
    return S_OK;
}

HRESULT OnRender() {
    ReturnIfFailed(commandAllocator->Reset());

    ReturnIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += SIZE_T(INT64(rtvDescriptorSize) * INT64(swapChain->GetCurrentBackBufferIndex()));

    float color[] = { 1.0f, 0.0f, 0.0f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, color, 0, nullptr);

    ReturnIfFailed(commandList->Close());

    ID3D12CommandList *commandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    ReturnIfFailed(swapChain->Present(1, 0));

    return S_OK;
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        if (FAILED(OnUpdate())) SendMessage(hwnd, WM_DESTROY, 0, 0);
        if (FAILED(OnRender())) SendMessage(hwnd, WM_DESTROY, 0, 0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
