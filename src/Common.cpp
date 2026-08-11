#include "Common.h"

#include <comdef.h>
#include <cstdio>

bool CheckHR(HRESULT hr, const wchar_t* what, const wchar_t* file, int line)
{
    if (SUCCEEDED(hr))
        return true;

    // _com_error가 HRESULT를 사람이 읽을 수 있는 문장으로 바꿔준다.
    // 예: 0x887A0005 -> "The GPU device instance has been suspended..."
    const _com_error err(hr);

    wchar_t message[1024]{};
    swprintf_s(message,
               L"%s 실패\n\n"
               L"HRESULT : 0x%08X\n"
               L"내용    : %s\n"
               L"위치    : %s(%d)",
               what, static_cast<unsigned>(hr), err.ErrorMessage(), file, line);

    MessageBoxW(nullptr, message, L"MiniD3D11Renderer", MB_OK | MB_ICONERROR);
    return false;
}
