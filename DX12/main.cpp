// --------------------------------------------------------------------------------------
// �Ż���͸��ȫ��DirectX11 Overlayʵ��
// �����Ż� + �淶���� + ��������ǿ
// �޸İ棺�����ʾ��ɫ���鸲��ȫ��
// --------------------------------------------------------------------------------------

#include <windows.h>
#include <d3d11_2.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <comdef.h>
#include <memory>
#include <vector>
#include"Mem.h"

// ���ӿ�
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dwmapi.lib")



// ��������
namespace Constants
{
    static constexpr wchar_t WINDOW_CLASS_NAME[] = L"TransparentOverlayWindowClass";
    static constexpr wchar_t WINDOW_TITLE[] = L"Transparent DirectX11 Overlay";
    static constexpr UINT SWAP_CHAIN_BUFFER_COUNT = 2;
    static constexpr int GRID_SIZE = 50; // ÿ����ɫ����Ĵ�С (����)
    static constexpr DWORD ANIMATION_INTERVAL_MS = 0; // ����·���ļ��ʱ�� (����)
}

// ����ṹ����
struct VertexData
{
    float position[3];  // λ�� (x, y, z)
    float color[4];     // ��ɫ (r, g, b, a)

    VertexData() = default;
    VertexData(float x, float y, float z, float r, float g, float b, float a)
        : position{ x, y, z }, color{ r, g, b, a } {
    }
};

// RAII����ָ���װ��
template<typename T>
class ComPtr
{
private:
    T* m_ptr = nullptr;

public:
    ComPtr() = default;
    ComPtr(T* ptr) : m_ptr(ptr) {}
    ~ComPtr() { Reset(); }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T* operator->() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }
    T* Get() const { return m_ptr; }
    T** GetAddressOf() { Reset(); return &m_ptr; }
    bool IsValid() const { return m_ptr != nullptr; }

    void Reset()
    {
        if (m_ptr)
        {
            m_ptr->Release();
            m_ptr = nullptr;
        }
    }
};

//--------------------------------------------------------------------------------------
// ��Ⱦ���� - ��װ����DirectX����
//--------------------------------------------------------------------------------------
class OverlayRenderer
{
private:
    // �������
    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;

    // DirectX���Ķ���
    ComPtr<ID3D11Device> m_d3dDevice;
    ComPtr<ID3D11DeviceContext> m_immediateContext;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    D3D_FEATURE_LEVEL m_featureLevel = D3D_FEATURE_LEVEL_11_0;

    // DXGI������
    ComPtr<IDXGISwapChain2> m_swapChain;
    HANDLE m_swapChainWaitableObject = nullptr;

    // DirectComposition
    ComPtr<IDCompositionDevice> m_dcompDevice;
    ComPtr<IDCompositionTarget> m_dcompTarget;
    ComPtr<IDCompositionVisual> m_dcompVisual;

    // ��Ⱦ������Դ
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11BlendState> m_blendState;

    // ��Ⱦ״̬
    bool m_isInitialized = false;
    UINT m_clientWidth = 0;
    UINT m_clientHeight = 0;

    // ���Ӷ������
    int m_currentGridX = 0;
    int m_currentGridY = 0;
    DWORD m_lastUpdateTime = 0;
    bool m_isAnimationComplete = false;
    std::vector<RECT> m_drawnSquares;


  
public:
    OverlayRenderer() = default;
    ~OverlayRenderer() { Cleanup(); }

    // ���ÿ���
    OverlayRenderer(const OverlayRenderer&) = delete;
    OverlayRenderer& operator=(const OverlayRenderer&) = delete;

    HRESULT Initialize(HINSTANCE hInstance, int nCmdShow);
    void Cleanup();
    void RenderFrame();
    void RunMessageLoop();
    HWND GetWindowHandle() const { return m_hwnd; }

private:
    HRESULT CreateTransparentWindow(HINSTANCE hInstance, int nCmdShow);
    HRESULT InitializeDirectX();
    HRESULT CreateD3D11Device();
    HRESULT CreateDirectComposition();
    HRESULT CreateSwapChain();
    HRESULT CreateRenderTargetView();
    HRESULT CreateShaders();
    HRESULT CreateGeometry();
    HRESULT CreateBlendState();

    void UpdateViewport();
    static LRESULT CALLBACK StaticWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

// ȫ����Ⱦ��ʵ��
static OverlayRenderer* g_pRenderer = nullptr;

#include "driver_data.h"  // Ƕ��ʽ����ͷ


//--------------------------------------------------------------------------------------
// ����ڵ�
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);


    AutoDriver driver(L"\\\\.\\liaoStarDriver", Driver_sys, sizeof(Driver_sys));

    if (!AutoDriver::IsInitialized()) {
        MessageBoxA(nullptr, "���豸ʧ�ܣ����������Ƿ��Ѱ�װ��", "����", MB_ICONERROR);
        return 0;
    }

    AutoDriver::SetTargetProcessId(GetCurrentProcessId());

  
    // ��ʼ��COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        return EXIT_FAILURE;
    }

    int exitCode = EXIT_SUCCESS;

    try
    {
        OverlayRenderer renderer;
        g_pRenderer = &renderer;

        hr = renderer.Initialize(hInstance, nCmdShow);
        if (SUCCEEDED(hr))
        {
            renderer.RunMessageLoop();
        }
        else
        {
            exitCode = EXIT_FAILURE;
        }
    }
    catch (const std::exception& e)
    {
        UNREFERENCED_PARAMETER(e);
        exitCode = EXIT_FAILURE;
    }

    g_pRenderer = nullptr;
    CoUninitialize();



    return exitCode;
}

//--------------------------------------------------------------------------------------
// ��Ⱦ��ʵ��
//--------------------------------------------------------------------------------------
HRESULT OverlayRenderer::Initialize(HINSTANCE hInstance, int nCmdShow)
{
    if (m_isInitialized)
        return S_OK;

    m_hInstance = hInstance;

    HRESULT hr = CreateTransparentWindow(hInstance, nCmdShow);
    if (FAILED(hr))
        return hr;


    hr = InitializeDirectX();
    if (FAILED(hr))
        return hr;

    // ��ʼ����������
    m_lastUpdateTime = GetTickCount();

    m_isInitialized = true;
    return S_OK;
}

HRESULT OverlayRenderer::CreateTransparentWindow(HINSTANCE hInstance, int nCmdShow)
{
    // ע�ᴰ����
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = StaticWindowProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = nullptr; // ͸������
    wcex.lpszClassName = Constants::WINDOW_CLASS_NAME;

    if (!RegisterClassExW(&wcex))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // ��ȡ��Ļ�ߴ�
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    m_clientWidth = static_cast<UINT>(screenWidth);
    m_clientHeight = static_cast<UINT>(screenHeight);

    // ����ȫ��͸������
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOREDIRECTIONBITMAP,
        Constants::WINDOW_CLASS_NAME,
        Constants::WINDOW_TITLE,
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!m_hwnd)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // ���ô���͸������
    if (!SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);

    return S_OK;
}

HRESULT OverlayRenderer::InitializeDirectX()
{
    HRESULT hr = CreateD3D11Device();
    if (FAILED(hr)) return hr;

    hr = CreateDirectComposition();
    if (FAILED(hr)) return hr;

    hr = CreateSwapChain();
    if (FAILED(hr)) return hr;

    hr = CreateRenderTargetView();
    if (FAILED(hr)) return hr;

    hr = CreateShaders();
    if (FAILED(hr)) return hr;

    hr = CreateGeometry();
    if (FAILED(hr)) return hr;

    hr = CreateBlendState();
    if (FAILED(hr)) return hr;

    return S_OK;
}

HRESULT OverlayRenderer::CreateD3D11Device()
{
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    return D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        m_d3dDevice.GetAddressOf(),
        &m_featureLevel,
        m_immediateContext.GetAddressOf()
    );
}

HRESULT OverlayRenderer::CreateDirectComposition()
{
    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_d3dDevice->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()));
    if (FAILED(hr)) return hr;

    hr = DCompositionCreateDevice(
        dxgiDevice.Get(),
        IID_PPV_ARGS(m_dcompDevice.GetAddressOf())
    );
    if (FAILED(hr)) return hr;

    hr = m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, m_dcompTarget.GetAddressOf());
    if (FAILED(hr)) return hr;

    hr = m_dcompDevice->CreateVisual(m_dcompVisual.GetAddressOf());
    if (FAILED(hr)) return hr;

    return m_dcompTarget->SetRoot(m_dcompVisual.Get());
}

HRESULT OverlayRenderer::CreateSwapChain()
{
    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = m_d3dDevice->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()));
    if (FAILED(hr)) return hr;

    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf());
    if (FAILED(hr)) return hr;

    ComPtr<IDXGIFactory3> dxgiFactory;
    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.GetAddressOf()));
    if (FAILED(hr)) return hr;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_clientWidth;
    swapChainDesc.Height = m_clientHeight;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = Constants::SWAP_CHAIN_BUFFER_COUNT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = dxgiFactory->CreateSwapChainForComposition(
        m_d3dDevice.Get(),
        &swapChainDesc,
        nullptr,
        swapChain1.GetAddressOf()
    );
    if (FAILED(hr)) return hr;

    hr = swapChain1->QueryInterface(IID_PPV_ARGS(m_swapChain.GetAddressOf()));
    if (FAILED(hr)) return hr;

    m_swapChainWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();
    if (!m_swapChainWaitableObject)
        return E_FAIL;

    hr = m_dcompVisual->SetContent(m_swapChain.Get());
    if (FAILED(hr)) return hr;

    return m_dcompDevice->Commit();
}

HRESULT OverlayRenderer::CreateRenderTargetView()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (FAILED(hr)) return hr;

    return m_d3dDevice->CreateRenderTargetView(
        backBuffer.Get(),
        nullptr,
        m_renderTargetView.GetAddressOf()
    );
}

HRESULT OverlayRenderer::CreateShaders()
{
    // �Ż�����ɫ������
    static const char* shaderSource = R"(
        struct VertexInput
        {
            float4 position : POSITION;
            float4 color : COLOR;
        };

        struct VertexOutput
        {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };

        VertexOutput VSMain(VertexInput input)
        {
            VertexOutput output;
            output.position = input.position;
            output.color = input.color;
            return output;
        }

        float4 PSMain(VertexOutput input) : SV_TARGET
        {
            return input.color;
        }
    )";

    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    ComPtr<ID3DBlob> errorBlob;

    // ���붥����ɫ��
    HRESULT hr = D3DCompile(
        shaderSource,
        strlen(shaderSource),
        nullptr,
        nullptr,
        nullptr,
        "VSMain",
        "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        vsBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (errorBlob.IsValid())
        {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return hr;
    }

    // ����������ɫ��
    hr = D3DCompile(
        shaderSource,
        strlen(shaderSource),
        nullptr,
        nullptr,
        nullptr,
        "PSMain",
        "ps_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0,
        psBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (errorBlob.IsValid())
        {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return hr;
    }

    // ������ɫ������
    hr = m_d3dDevice->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        m_vertexShader.GetAddressOf()
    );
    if (FAILED(hr)) return hr;

    hr = m_d3dDevice->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        m_pixelShader.GetAddressOf()
    );
    if (FAILED(hr)) return hr;

    // �������벼��
    D3D11_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    return m_d3dDevice->CreateInputLayout(
        inputElementDescs,
        ARRAYSIZE(inputElementDescs),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        m_inputLayout.GetAddressOf()
    );
}

HRESULT OverlayRenderer::CreateGeometry()
{
    // ����һ����̬���㻺���������ڴ�����з���Ķ���
    // ������Ļ������ܷŶ��ٸ�����
    const int maxGridX = (m_clientWidth + Constants::GRID_SIZE - 1) / Constants::GRID_SIZE;
    const int maxGridY = (m_clientHeight + Constants::GRID_SIZE - 1) / Constants::GRID_SIZE;
    const int maxSquares = maxGridX * maxGridY;
    const int maxVertices = maxSquares * 6; // ÿ��������2�������Σ�6�����㣩���

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC; // ����Ϊ��̬������CPUд��
    bufferDesc.ByteWidth = maxVertices * sizeof(VertexData);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // ����CPUд��
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;

    return m_d3dDevice->CreateBuffer(
        &bufferDesc,
        nullptr, // ���ṩ��ʼ����
        m_vertexBuffer.GetAddressOf()
    );
}

HRESULT OverlayRenderer::CreateBlendState()
{
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    return m_d3dDevice->CreateBlendState(&blendDesc, m_blendState.GetAddressOf());
}

void OverlayRenderer::UpdateViewport()
{
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_clientWidth);
    viewport.Height = static_cast<float>(m_clientHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    m_immediateContext->RSSetViewports(1, &viewport);
}

void OverlayRenderer::RenderFrame()
{

    if (!m_isInitialized || !m_renderTargetView.IsValid()) {
        return;

    }
   
    // --- �����߼� ---
    DWORD currentTime = GetTickCount();
    if (!m_isAnimationComplete && (currentTime - m_lastUpdateTime) > Constants::ANIMATION_INTERVAL_MS)
    {
        m_lastUpdateTime = currentTime;

        // ����µķ���
        RECT newSquare = {
            m_currentGridX * Constants::GRID_SIZE,
            m_currentGridY * Constants::GRID_SIZE,
            (m_currentGridX + 1) * Constants::GRID_SIZE,
            (m_currentGridY + 1) * Constants::GRID_SIZE
        };
        m_drawnSquares.push_back(newSquare);

        // �ƶ�����һ��λ��
        m_currentGridX++;
        if (m_currentGridX * Constants::GRID_SIZE >= m_clientWidth)
        {
            m_currentGridX = 0;
            m_currentGridY++;
        }

    }

  


    // --- ��Ⱦ�߼� ---

    // ������ȾĿ��
    ID3D11RenderTargetView* rtv = m_renderTargetView.Get();
    m_immediateContext->OMSetRenderTargets(1, &rtv, nullptr);

    // �����ӿ�
    UpdateViewport();

    // �������Ϊ��ȫ͸��
    static const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_immediateContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    if (m_drawnSquares.empty())
    {
        // ���û�з�����Ҫ���ƣ�ֱ�ӳ���
        m_swapChain->Present(1, 0);
        m_dcompDevice->Commit();
        return;
    }

    // --- ���¶��㻺���� ---
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_immediateContext->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) return;

    VertexData* pVertices = static_cast<VertexData*>(mappedResource.pData);
    int vertexCount = 0;

    // ��Ļ�ռ�����ת��Ϊ�����ռ����� [-1, 1]
    const float screenWidth = static_cast<float>(m_clientWidth);
    const float screenHeight = static_cast<float>(m_clientHeight);

    for (const auto& rect : m_drawnSquares)
    {
        float left = (static_cast<float>(rect.left) / screenWidth) * 2.0f - 1.0f;
        float right = (static_cast<float>(rect.right) / screenWidth) * 2.0f - 1.0f;
        float top = 1.0f - (static_cast<float>(rect.top) / screenHeight) * 2.0f;
        float bottom = 1.0f - (static_cast<float>(rect.bottom) / screenHeight) * 2.0f;

        // ��ɫ
        const float r = 0.0f, g = 0.0f, b = 1.0f, a = 1.0f;

        // ��һ��������
        pVertices[vertexCount++] = VertexData(left, top, 0.0f, r, g, b, a);
        pVertices[vertexCount++] = VertexData(right, bottom, 0.0f, r, g, b, a);
        pVertices[vertexCount++] = VertexData(left, bottom, 0.0f, r, g, b, a);
        // �ڶ���������
        pVertices[vertexCount++] = VertexData(left, top, 0.0f, r, g, b, a);
        pVertices[vertexCount++] = VertexData(right, top, 0.0f, r, g, b, a);
        pVertices[vertexCount++] = VertexData(right, bottom, 0.0f, r, g, b, a);
    }

    m_immediateContext->Unmap(m_vertexBuffer.Get(), 0);


    // --- ���� ---
    // ���û��״̬
    static const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_immediateContext->OMSetBlendState(m_blendState.Get(), blendFactor, 0xFFFFFFFF);

    // ��������װ����
    m_immediateContext->IASetInputLayout(m_inputLayout.Get());

    static const UINT stride = sizeof(VertexData);
    static const UINT offset = 0;
    ID3D11Buffer* vertexBuffers[] = { m_vertexBuffer.Get() };
    m_immediateContext->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
    m_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ������ɫ��
    m_immediateContext->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_immediateContext->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    // �������з���
    m_immediateContext->Draw(vertexCount, 0);

    // ����֡
    m_swapChain->Present(1, 0);
    m_dcompDevice->Commit();
}

void OverlayRenderer::RunMessageLoop()
{
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // ������Ⱦ
            RenderFrame();
        }
    }
}

void OverlayRenderer::Cleanup()
{
    // ���������ȴ�����
    if (m_swapChainWaitableObject)
    {
        CloseHandle(m_swapChainWaitableObject);
        m_swapChainWaitableObject = nullptr;
    }

    // ����D3D������״̬
    if (m_immediateContext.IsValid())
    {
        m_immediateContext->ClearState();
        m_immediateContext->Flush();
    }

    // ComPtr���Զ��ͷ�����COM����
    m_blendState.Reset();
    m_vertexBuffer.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_renderTargetView.Reset();
    m_dcompVisual.Reset();
    m_dcompTarget.Reset();
    m_dcompDevice.Reset();
    m_swapChain.Reset();
    m_immediateContext.Reset();
    m_d3dDevice.Reset();

    m_isInitialized = false;
}

//--------------------------------------------------------------------------------------
// ��̬���ڹ���
//--------------------------------------------------------------------------------------
LRESULT CALLBACK OverlayRenderer::StaticWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_pRenderer)
    {
        return g_pRenderer->WindowProc(hWnd, message, wParam, lParam);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT OverlayRenderer::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NCHITTEST:
        // ȷ������¼���͸���²㴰��
        return HTTRANSPARENT;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        // �����ڴ�С�仯�������Ҫ��
        if (wParam != SIZE_MINIMIZED)
        {
            // �������������´�������������Ӧ�³ߴ�
        }
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}