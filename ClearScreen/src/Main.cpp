#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

inline void ThrowIfFailed(HRESULT hr, LPCWSTR file, int line) {
    if (FAILED(hr)) {
        TCHAR buffer[256];
        wsprintf(buffer, TEXT("\nFILE:[%s]\nLINE:[%d]\n\n"), file, line);
        OutputDebugString(buffer);
        throw hr;
    }
}

#define ThrowIfFailed(hr) ThrowIfFailed(hr, __FILEW__, __LINE__);

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
ComPtr<ID3D12Fence> fence;
UINT64 fenceValue;
HANDLE fenceEvent;
UINT frameIndex;

bool upping[] = { true, true, true };
float color[] = { 1.0f, 0.5f, 0.0f, 1.0f };

HRESULT InitWindow();
HRESULT InitDirectX();
void OnUpdate();
void OnRender();
void WaitForPrevFrame();
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
    ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory4)));

    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(factory4->QueryInterface(IID_PPV_ARGS(&factory6)))) {
        factory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
    } else {
        factory4->EnumAdapters1(0, &adapter);
    }

    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

    // Command Queue
    {
        D3D12_COMMAND_QUEUE_DESC desc;
        desc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 0;
        ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)));
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
        ThrowIfFailed(factory4->CreateSwapChainForHwnd(
            commandQueue.Get(),
            hWindow,
            &desc,
            nullptr,
            nullptr,
            &swapChain1
        ));

        ThrowIfFailed(swapChain1.As(&swapChain));
        frameIndex = swapChain->GetCurrentBackBufferIndex();
    }

    // Descriptor Heap for RTV
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc;
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = FrameCount;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask       = 0;

        ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap)));

        rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(desc.Type);
    }

    // Render Target View (RTV)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

        for (UINT i = 0; i < FrameCount; i++) {
            ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i])));
            device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);

            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    // Command Allocator
    ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));

    // Command List
    ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
    ThrowIfFailed(commandList->Close());

    // Fence
    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    fenceEvent = CreateEvent(nullptr, false, false, nullptr);
    if (!fenceEvent) {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    return S_OK;
}

void OnUpdate() {
    for (int i = 0; i < 3; i++) {
        color[i] += upping[i] ? 0.01f : -0.01f;
        if (color[i] < 0.0f) {
            upping[i] = true;
        } else if (color[i] > 1.0f) {
            upping[i] = false;
        }
    }
}

void OnRender() {
    ThrowIfFailed(commandAllocator->Reset());

    ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));

    // Resource Barrier
    {
        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = renderTargets[frameIndex].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        commandList->ResourceBarrier(1, &barrier);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += SIZE_T(INT64(rtvDescriptorSize) * INT64(frameIndex));

    commandList->ClearRenderTargetView(rtvHandle, color, 0, nullptr);

    // Resource Barrier
    {
        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = renderTargets[frameIndex].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        commandList->ResourceBarrier(1, &barrier);
    }

    ThrowIfFailed(commandList->Close());

    ID3D12CommandList *commandLists[] = { commandList.Get() };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    ThrowIfFailed(swapChain->Present(1, 0));

    WaitForPrevFrame();
}

void WaitForPrevFrame() {
    ThrowIfFailed(commandQueue->Signal(fence.Get(), ++fenceValue));

    if (fence->GetCompletedValue() < fenceValue) {
        ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
        WaitForSingleObject(fenceEvent, INFINITE);
    }

    frameIndex = swapChain->GetCurrentBackBufferIndex();
}

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
