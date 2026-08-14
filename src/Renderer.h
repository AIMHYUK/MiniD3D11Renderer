#pragma once

#include "Common.h"

#include <d3d11.h>
#include <dxgi1_2.h>

// 정점 하나가 담는 것. C++ 쪽 정의다.
//
// 이 구조체와 HLSL의 VSInput, 그리고 입력 레이아웃 셋이 서로 맞아야 한다.
// 하나라도 어긋나면 그림이 엉뚱한 데 뜨거나 아예 안 뜬다.
struct Vertex
{
    float x, y;          // NDC 좌표. 화면 중앙이 (0,0), 범위는 -1 ~ +1
    float r, g, b, a;    // 색. 각 0 ~ 1
};

// D3D11 디바이스·스왑체인·백버퍼 뷰를 소유하고, 한 프레임을 그린다.
class Renderer
{
public:
    bool Initialize(HWND hwnd, UINT width, UINT height);

    // 창 크기가 바뀌었을 때 스왑체인 버퍼를 다시 잡는다.
    void Resize(UINT width, UINT height);

    void Render();

private:
    bool CreateDeviceAndSwapChain(HWND hwnd, UINT width, UINT height);
    bool CreateBackBufferView();
    void ReleaseBackBufferView();

    // 스프라이트를 그리는 데 필요한 것들을 한 번에 만든다.
    // 셰이더 컴파일 -> 셰이더 객체 -> 입력 레이아웃 -> 정점/인덱스 버퍼 -> 래스터라이저 상태 순.
    bool CreateSpriteResources();

    ComPtr<ID3D11Device>           m_device;
    ComPtr<ID3D11DeviceContext>    m_context;
    ComPtr<IDXGISwapChain1>        m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_backBufferView;

    // ── 스프라이트용 ──
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader>  m_pixelShader;
    ComPtr<ID3D11InputLayout>  m_inputLayout;   // 정점 버퍼의 바이트를 해석하는 규칙
    ComPtr<ID3D11Buffer>       m_vertexBuffer;  // GPU 메모리에 올라간 정점 데이터
    ComPtr<ID3D11Buffer>       m_indexBuffer;   // 그 정점을 몇 번째로 읽을지의 목록
    ComPtr<ID3D11RasterizerState> m_rasterizerState; // 컬링 규칙(앞뒤 판정, 양면 그리기)을 여기서 정한다

    UINT m_width  = 0;
    UINT m_height = 0;
};
