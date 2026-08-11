#include "Common.h"
#include "FrameTimer.h"
#include "Renderer.h"
#include "Window.h"

#include <cstdio>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    constexpr UINT kInitialWidth  = 1280;
    constexpr UINT kInitialHeight = 720;

    Window window;
    if (!window.Create(hInstance, L"MiniD3D11Renderer", kInitialWidth, kInitialHeight))
        return -1;

    Renderer renderer;
    if (!renderer.Initialize(window.Handle(), window.Width(), window.Height()))
        return -1;

    // Window는 Renderer의 존재를 모른다. 크기가 바뀌었다는 사실만 알리고,
    // 그것으로 무엇을 할지는 여기서 연결한다.
    window.SetResizeCallback([&renderer](UINT width, UINT height) {
        renderer.Resize(width, height);
    });

    window.Show(nCmdShow);

    FrameTimer timer;

    // 게임 루프. 메시지를 모두 처리한 뒤 한 프레임을 그린다.
    while (window.PumpMessages())
    {
        timer.Tick();
        renderer.Render();

        if (double fps = 0.0; timer.TryGetAverageFps(fps))
        {
            wchar_t title[128]{};
            swprintf_s(title, L"MiniD3D11Renderer  |  %u x %u  |  %.1f FPS",
                       window.Width(), window.Height(), fps);
            SetWindowTextW(window.Handle(), title);
        }
    }

    return 0;
}
