#include "Renderer.h"

#include <cstdint>
#include <d3dcompiler.h>
#include <dxgi1_2.h>

namespace
{
    // 백버퍼 포맷. 플립 모델 스왑체인이 허용하는 포맷 중 하나다.
    constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    // 화면을 지울 색. 아무것도 안 그려도 이 색이 보이면 파이프라인이 살아 있다는 뜻이다.
    constexpr float kClearColor[4] = { 1.00f, 0.11f, 0.16f, 1.0f };

    // HLSL 파일 하나를 컴파일해서 결과 바이트코드를 blob에 담아 준다.
    //
    // 셰이더는 C++처럼 미리 exe에 박히는 게 아니라, 텍스트 상태로 두었다가
    // 실행할 때 GPU가 알아듣는 바이트코드로 번역한다. 그 번역기가 D3DCompileFromFile이다.
    // (미리 fxc.exe로 번역해 .cso 파일로 만들어 두는 방법도 있다. 나중에 바꿀 수 있다)
    bool CompileShaderFromFile(const wchar_t* fileName,
                               const char* entryPoint,
                               const char* target,        // "vs_5_0" / "ps_5_0"
                               ComPtr<ID3DBlob>& outBlob)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        // 디버그 빌드에서는 최적화를 끄고 디버그 정보를 넣는다.
        // 그래야 그래픽 디버거에서 셰이더를 한 줄씩 따라갈 수 있다.
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        const std::wstring fullPath = ResolvePathFromExe(fileName);

        ComPtr<ID3DBlob> errorBlob;
        const HRESULT hr = D3DCompileFromFile(
            fullPath.c_str(),
            nullptr,                            // 매크로 정의 없음
            D3D_COMPILE_STANDARD_FILE_INCLUDE,  // #include를 파일 기준으로 처리
            entryPoint,
            target,
            flags, 0,
            outBlob.GetAddressOf(),
            errorBlob.GetAddressOf());

        if (FAILED(hr))
        {
            // 셰이더 오류는 HRESULT만 봐서는 아무것도 알 수 없다.
            // 진짜 정보("몇 번째 줄에서 무슨 문법 오류")는 errorBlob 안에 문장으로 들어 있다.
            // 이걸 안 보여주면 셰이더 디버깅이 불가능해진다.
            if (errorBlob)
            {
                const char* text = static_cast<const char*>(errorBlob->GetBufferPointer());
                MessageBoxA(nullptr, text, "셰이더 컴파일 오류", MB_OK | MB_ICONERROR);
            }
            else
            {
                // errorBlob이 없으면 대개 파일을 못 찾은 것이다.
                std::wstring message = L"셰이더 파일을 열 수 없습니다.\n\n";
                message += fullPath;
                MessageBoxW(nullptr, message.c_str(), L"MiniD3D11Renderer", MB_OK | MB_ICONERROR);
            }
            return false;
        }

        return true;
    }
}

bool Renderer::Initialize(HWND hwnd, UINT width, UINT height)
{
    m_width  = width;
    m_height = height;

    if (!CreateDeviceAndSwapChain(hwnd, width, height))
        return false;

    if (!CreateBackBufferView())
        return false;

    return CreateSpriteResources();
}

bool Renderer::CreateSpriteResources()
{
    // ── 1. 셰이더 소스를 GPU 바이트코드로 컴파일한다 ──
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;

    if (!CompileShaderFromFile(L"shaders\\Sprite.vs.hlsl", "main", "vs_5_0", vsBlob))
        return false;
    if (!CompileShaderFromFile(L"shaders\\Sprite.ps.hlsl", "main", "ps_5_0", psBlob))
        return false;

    // ── 2. 바이트코드로 실제 셰이더 객체를 만든다 ──
    // blob은 그냥 바이트 덩어리고, 이걸 GPU가 쓸 수 있는 객체로 바꾸는 단계다.
    if (!HR_CHECK(m_device->CreateVertexShader(
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr,
            m_vertexShader.GetAddressOf()),
            L"CreateVertexShader"))
        return false;

    if (!HR_CHECK(m_device->CreatePixelShader(
            psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr,
            m_pixelShader.GetAddressOf()),
            L"CreatePixelShader"))
        return false;

    // ── 3. 입력 레이아웃 ──
    //
    // GPU 입장에서 정점 버퍼는 그냥 바이트 뭉치다. 어디서부터 어디까지가 위치이고
    // 색인지 알 방법이 없다. 그 해석 규칙을 알려주는 것이 입력 레이아웃이다.
    //
    // Vertex 구조체가 메모리에 이렇게 깔린다:
    //
    //   바이트  0   4   8      12  16  20   24
    //          [x] [y] [r] [g] [b] [a]
    //          └ POSITION ┘ └──── COLOR ────┘
    //           (8바이트)        (16바이트)
    //
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        // 시맨틱 이름, 인덱스, 포맷, 입력 슬롯, 시작 바이트 오프셋, 분류, 인스턴스 스텝
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    // 만들 때 정점 셰이더 바이트코드를 같이 넘긴다.
    // 런타임이 "이 레이아웃이 저 셰이더의 입력과 실제로 맞는가"를 여기서 검증한다.
    // 시맨틱 이름을 오타 내면 바로 여기서 실패한다.
    if (!HR_CHECK(m_device->CreateInputLayout(
            layout, _countof(layout),
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
            m_inputLayout.GetAddressOf()),
            L"CreateInputLayout"))
        return false;

    // ── 4. 정점 버퍼 ──
    //
    // 좌표는 NDC다. 화면 중앙이 (0,0), 위가 +y, 범위는 -1 ~ +1.
    // 창 크기가 바뀌어도 이 값은 그대로라서 사각형이 같이 늘어난다.
    //
    // 사각형은 삼각형 2개지만 정점은 4개면 된다. 0과 2가 두 삼각형에 함께 쓰인다.
    //
    //   0 ────── 1
    //   │ ╲      │        위쪽 삼각형: 0, 1, 2
    //   │   ╲    │        아래쪽 삼각형: 0, 2, 3
    //   3 ────── 2
    const Vertex vertices[] = {
        //   x      y        r     g     b     a
        { -0.5f,  0.5f,    1.0f, 0.2f, 0.2f, 1.0f },   // 0 좌상 - 빨강
        {  0.5f,  0.5f,    1.0f, 1.0f, 0.2f, 1.0f },   // 1 우상 - 노랑
        {  0.5f, -0.5f,    0.2f, 1.0f, 0.2f, 1.0f },   // 2 우하 - 초록
        { -0.5f, -0.5f,    0.2f, 0.4f, 1.0f, 1.0f },   // 3 좌하 - 파랑
    };

    // 인덱스 버퍼는 정점 버퍼를 대체하는 것이 아니라 "몇 번 정점을 어떤 순서로 읽을지"의
    // 목록이다. 둘 다 있어야 한다.
    //
    // 순서가 시계 방향인 것에 이유가 있다. D3D는 기본적으로 시계 방향을 앞면으로 보고,
    // 뒷면은 그리지 않고 버린다(백페이스 컬링). 반시계로 적으면 삼각형이 사라진다.
    // 정확히는 순환 방향이 기준이라 { 0,2,3 }과 { 2,3,0 }은 같은 결과다.
    const uint16_t indices[] = {
        0, 1, 2,    // 위쪽 삼각형
        0, 2, 3,    // 아래쪽 삼각형
    };

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = sizeof(vertices);
    // IMMUTABLE = 만든 뒤 절대 안 바뀐다. GPU가 가장 빠른 메모리에 둘 수 있다.
    // 매 프레임 내용을 바꿔야 하면 DYNAMIC + CPU_ACCESS_WRITE를 쓴다. (6일차에 그렇게 한다)
    desc.Usage     = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initialData{};
    initialData.pSysMem = vertices;

    if (!HR_CHECK(m_device->CreateBuffer(&desc, &initialData, m_vertexBuffer.GetAddressOf()),
                  L"CreateBuffer(VertexBuffer)"))
        return false;

    // 인덱스 버퍼도 만드는 절차가 정점 버퍼와 똑같다. BindFlags만 다르다.
    D3D11_BUFFER_DESC indexDesc{};
    indexDesc.ByteWidth = sizeof(indices);
    indexDesc.Usage     = D3D11_USAGE_IMMUTABLE;
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData{};
    indexData.pSysMem = indices;

    if (!HR_CHECK(m_device->CreateBuffer(&indexDesc, &indexData, m_indexBuffer.GetAddressOf()),
                  L"CreateBuffer(IndexBuffer)"))
        return false;

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = FALSE;

    if (!HR_CHECK(m_device->CreateRasterizerState(&rd, m_rasterizerState.GetAddressOf()),
        L"CreateRasterizerState"))
        return false;


    return true;
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

    // ── 사각형 그리기 ──
    //
    // DrawIndexed()에는 "무엇을 어디에 어떻게"가 하나도 안 들어간다. 개수만 넘긴다.
    // 그래서 그 전에 파이프라인 각 자리에 필요한 것을 꽂아둬야 한다.
    // 아래 여섯 줄이 그 꽂아두는 작업이고, 마지막 한 줄이 "그려"다.

    UINT stride = sizeof(Vertex);   // 정점 하나가 몇 바이트인지
    UINT offset = 0;                // 버퍼 앞에서 몇 바이트 건너뛰고 시작할지

    // IA — 바이트를 어떻게 해석할지, 어느 버퍼에서 읽을지, 셋씩 묶어 삼각형으로 볼지
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    // 정점 버퍼는 슬롯 여러 개에 꽂을 수 있어 배열로 넘기고(GetAddressOf),
    // 인덱스 버퍼는 파이프라인에 하나뿐이라 그냥 넘긴다(Get).
    // 두 번째 인자는 인덱스 하나의 크기다. indices가 uint16_t라 R16_UINT.
    m_context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    //D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP로 수정하니 선만 생김.
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->RSSetState(m_rasterizerState.Get());
    // VS / PS — 어떤 셰이더를 쓸지
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    // 그려. 인덱스 6개를 0번부터. 세 개씩 끊겨 삼각형 2개가 된다.
    //   Draw(2, 0)     -> 3개씩 끊는데 2개뿐이라 아무것도 안 나왔다
    //   DrawIndexed(3, 0, 0) -> 위쪽 삼각형만
    //   DrawIndexed(3, 3, 0) -> 인덱스 3번부터 읽어 아래쪽 삼각형만
    m_context->DrawIndexed(6, 0, 0);

    // 첫 번째 인자가 SyncInterval이다. 1이면 수직동기(VSync)를 기다린다.
    // 지금은 화면 찢김 없이 보는 것이 목적이라 1로 둔다.
    // 나중에 드로우 콜 비용을 재는 단계에서는 0으로 바꿔야 한다.
    // VSync가 켜져 있으면 프레임 시간이 모니터 주사율에 고정돼 GPU 부하 차이가 묻히기 때문이다.
    m_swapChain->Present(1, 0);
}
