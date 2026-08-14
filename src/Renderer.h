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
    // 좌표가 아니라 "스프라이트 안에서의 비율"이다. 좌상단 (0,0), 우하단 (1,1).
    // 화면 어디에 얼마만 한 크기로 놓을지는 상수 버퍼가 정하고, 변환은 정점 셰이더가 한다.
    // 그래서 이 버퍼는 스프라이트가 몇 개든 하나면 되고, IMMUTABLE로 둘 수 있다.
    float x, y;
    float r, g, b, a;    // 색. 각 0 ~ 1
};

// 매 프레임 정점 셰이더로 넘기는 값. shaders/Sprite.vs.hlsl의 cbuffer와 짝이다.
//
// 상수 버퍼에는 규칙이 둘 있다.
//   A. 버퍼 전체 크기가 16의 배수여야 한다      -> 어기면 CreateBuffer가 실패한다
//   B. 변수 하나가 16바이트 경계를 걸치면 안 된다 -> 어겨도 아무도 안 잡아준다. 값만 밀린다
//
// float2끼리 짝지으면 8+8=16으로 맞아떨어져 B가 저절로 지켜진다.
// 끝의 padding은 A를 채우는 용도다. float4가 끼는 배치라면 중간에도 넣어야 한다.
struct SpriteConstants
{
    float screenWidth, screenHeight;   //  0 ~  7   창 크기 (픽셀)
    float posX, posY;                  //  8 ~ 15   스프라이트 좌상단 위치 (픽셀)
    float sizeX, sizeY;                // 16 ~ 23   스프라이트 크기 (픽셀)
    float padding[2];                  // 24 ~ 31
};
// 런타임에 그림이 깨지고 나서 찾는 것보다 컴파일 때 걸리는 편이 낫다.
static_assert(sizeof(SpriteConstants) % 16 == 0, "상수 버퍼는 16바이트 배수여야 한다");

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
    ComPtr<ID3D11Buffer>       m_constantBuffer;   // 그 정점을 몇 번째로 읽을지의 목록
    ComPtr<ID3D11RasterizerState> m_rasterizerState; // 컬링 규칙(앞뒤 판정, 양면 그리기)을 여기서 정한다

    

    UINT m_width  = 0;
    UINT m_height = 0;
};
