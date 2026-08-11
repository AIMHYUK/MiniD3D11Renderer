#include "Renderer.h"

#include <dxgi1_2.h>

namespace
{
    // 백버퍼 포맷. 플립 모델 스왑체인이 허용하는 포맷 중 하나다.
    constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    // 화면을 지울 색. 아무것도 안 그려도 이 색이 보이면 파이프라인이 살아 있다는 뜻이다.
    constexpr float kClearColor[4] = { 0.09f, 0.11f, 0.16f, 1.0f };
}

bool Renderer::Initialize(HWND hwnd, UINT width, UINT height)
{
    m_width  = width;
    m_height = height;

    if (!CreateDeviceAndSwapChain(hwnd, width, height))
        return false;

    return CreateBackBufferView();
}

bool Renderer::CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height)
{
    UINT deviceFlags = 0;
#ifdef _DEBUG
    // 디버그 레이어를 켜면 잘못된 API 사용이 출력 창에 문장으로 찍힌다.
    // "그림이 안 나온다"를 눈으로 추측하는 대신 원인을 읽을 수 있게 해주는 장치라
    // Debug 빌드에서는 항상 켜둔다. (Windows SDK의 그래픽 도구가 설치돼 있어야 한다)
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // 요청할 기능 수준. 앞에서부터 시도해 GPU가 지원하는 첫 번째가 선택된다.
    const D3D_FEATURE_LEVEL requestedLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D_FEATURE_LEVEL acquiredLevel{};

    // 디바이스와 스왑체인을 한 번에 만들어주는 D3D11CreateDeviceAndSwapChain도 있지만
    // 그 함수는 옛 방식(BitBlt 모델) 스왑체인만 만든다. 플립 모델을 쓰려면
    // 디바이스를 먼저 만들고, DXGI 팩토리로 스왑체인을 따로 만들어야 한다.
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // 기본 어댑터
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        deviceFlags,
        requestedLevels,
        _countof(requestedLevels),
        D3D11_SDK_VERSION,
        m_device.GetAddressOf(),
        &acquiredLevel,
        m_context.GetAddressOf());

#ifdef _DEBUG
    if (FAILED(hr))
    {
        // 그래픽 도구가 설치돼 있지 않으면 디버그 레이어 요청이 실패한다.
        // 그때는 디버그 레이어 없이 한 번 더 시도한다.
        deviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags,
            requestedLevels, _countof(requestedLevels), D3D11_SDK_VERSION,
            m_device.GetAddressOf(), &acquiredLevel, m_context.GetAddressOf());
    }
#endif

    if (!HR_CHECK(hr, L"D3D11CreateDevice"))
        return false;

    // 스왑체인을 만들려면 이 디바이스를 만든 바로 그 어댑터의 팩토리가 필요하다.
    // ID3D11Device -> IDXGIDevice -> IDXGIAdapter -> IDXGIFactory2 순으로 거슬러 올라간다.
    ComPtr<IDXGIDevice> dxgiDevice;
    if (!HR_CHECK(m_device.As(&dxgiDevice), L"ID3D11Device -> IDXGIDevice"))
        return false;

    ComPtr<IDXGIAdapter> adapter;
    if (!HR_CHECK(dxgiDevice->GetAdapter(adapter.GetAddressOf()), L"IDXGIDevice::GetAdapter"))
        return false;

    ComPtr<IDXGIFactory2> factory;
    if (!HR_CHECK(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf())), L"IDXGIAdapter::GetParent"))
        return false;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width       = width;
    desc.Height      = height;
    desc.Format      = kBackBufferFormat;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    // 플립 모델은 버퍼가 2개 이상이어야 한다.
    desc.BufferCount = 2;
    // FLIP_DISCARD는 백버퍼를 복사하지 않고 화면에 그대로 넘긴다.
    // 옛 DISCARD 방식은 매 프레임 백버퍼를 화면 버퍼로 복사했다.
    // 대신 제약이 붙는다 - 여기서 MSAA를 쓸 수 없고(SampleDesc.Count는 1이어야 한다),
    // Present 이후 렌더 타깃 바인딩이 풀린다.
    desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    desc.AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED;

    if (!HR_CHECK(factory->CreateSwapChainForHwnd(
            m_device.Get(), hwnd, &desc, nullptr, nullptr, m_swapChain.GetAddressOf()),
            L"IDXGIFactory2::CreateSwapChainForHwnd"))
        return false;

    // Alt+Enter로 DXGI가 멋대로 전체화면을 토글하지 못하게 막는다.
    // 전체화면 전환은 스왑체인을 다시 만드는 일이라, 직접 다루기 전까지는 꺼두는 편이 낫다.
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    return true;
}

bool Renderer::CreateBackBufferView()
{
    // 스왑체인이 들고 있는 백버퍼 텍스처를 꺼내, "여기에 그린다"는 뷰를 만든다.
    // 텍스처 자체가 아니라 뷰를 파이프라인에 바인딩한다.
    ComPtr<ID3D11Texture2D> backBuffer;
    if (!HR_CHECK(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())),
                  L"IDXGISwapChain::GetBuffer"))
        return false;

    if (!HR_CHECK(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_backBufferView.GetAddressOf()),
                  L"ID3D11Device::CreateRenderTargetView"))
        return false;

    // 뷰포트는 정규화된 좌표를 픽셀 좌표로 옮기는 규칙이다. 창 크기가 바뀌면 같이 바뀌어야 한다.
    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width    = static_cast<float>(m_width);
    viewport.Height   = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    return true;
}

void Renderer::ReleaseBackBufferView()
{
    // ResizeBuffers는 백버퍼를 참조하는 것이 하나라도 남아 있으면 실패한다.
    // 뷰를 놓는 것만으로는 부족하고, 컨텍스트에 바인딩된 상태도 풀어야 한다.
    m_backBufferView.Reset();
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_context->Flush();
}

void Renderer::Resize(UINT width, UINT height)
{
    if (!m_swapChain || (width == m_width && height == m_height))
        return;

    m_width  = width;
    m_height = height;

    ReleaseBackBufferView();

    // 0을 넘기면 창의 현재 크기를 쓰고, 포맷에 UNKNOWN을 넘기면 기존 포맷을 유지한다.
    if (!HR_CHECK(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0),
                  L"IDXGISwapChain::ResizeBuffers"))
        return;

    CreateBackBufferView();
}

void Renderer::Render()
{
    if (!m_backBufferView)
        return;

    // FLIP 모델에서는 Present 뒤에 렌더 타깃 바인딩이 풀린다.
    // 그래서 매 프레임 다시 바인딩한다.
    ID3D11RenderTargetView* views[] = { m_backBufferView.Get() };
    m_context->OMSetRenderTargets(1, views, nullptr);

    m_context->ClearRenderTargetView(m_backBufferView.Get(), kClearColor);

    // 여기에 앞으로 드로우 콜이 들어간다.

    // 첫 번째 인자가 SyncInterval이다. 1이면 수직동기(VSync)를 기다린다.
    // 지금은 화면 찢김 없이 보는 것이 목적이라 1로 둔다.
    // 나중에 드로우 콜 비용을 재는 단계에서는 0으로 바꿔야 한다.
    // VSync가 켜져 있으면 프레임 시간이 모니터 주사율에 고정돼 GPU 부하 차이가 묻히기 때문이다.
    m_swapChain->Present(1, 0);
}
