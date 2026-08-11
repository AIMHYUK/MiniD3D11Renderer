#pragma once

#include "Common.h"

#include <d3d11.h>
#include <dxgi1_2.h>

// D3D11 디바이스·스왑체인·백버퍼 뷰를 소유하고, 한 프레임을 그린다.
//
// 지금 단계에서 "그린다"는 화면을 한 가지 색으로 지우는 것뿐이다.
// 정점 버퍼와 셰이더는 다음 단계에서 붙인다.
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

    ComPtr<ID3D11Device>           m_device;
    ComPtr<ID3D11DeviceContext>    m_context;
    ComPtr<IDXGISwapChain1>        m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_backBufferView;

    UINT m_width  = 0;
    UINT m_height = 0;
};
