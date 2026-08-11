# MiniD3D11Renderer

엔진 없이 Win32와 Direct3D 11만으로 만드는 2D 스프라이트 렌더러입니다.

창 생성부터 디바이스·스왑체인 초기화, 셰이더 컴파일, 텍스처 로딩, 스프라이트 배칭까지
직접 구현하고, **개별 드로우 콜과 배칭이 스프라이트 몇 개부터 갈라지는지를 실측**하는 것이
이 프로젝트의 목적입니다.

## 왜 만들었는가

앞서 만든 [MiniOscReceiver](https://github.com/AIMHYUK/MiniOscReceiver)에서
언리얼의 OSC 플러그인 내부를 직접 구현해 RPC와 성능을 비교한 적이 있습니다.
그때 얻은 결론은 구조 선택의 근거는 측정에서 나온다는 것이었습니다.

이 프로젝트는 같은 방식을 렌더링 쪽에 적용합니다.
엔진이 알아서 해주던 드로우 콜 묶기를 직접 만들어보고,
"배칭이 빠르다"는 통념이 실제로 몇 개부터 유효해지는지를 숫자로 확인합니다.

## 환경

- Visual Studio 2022 (v143) / C++17 / x64
- Windows SDK 10 (`d3d11.lib`, `dxgi.lib`, `d3dcompiler.lib`)
- 외부 라이브러리 없음. 텍스처 로딩도 Windows 자체 기능(WIC)을 씁니다.

## 빌드

`MiniD3D11Renderer.sln`을 Visual Studio 2022로 열고 x64 / Debug 또는 Release로 빌드합니다.
산출물은 `build/x64/<Configuration>/`에 생깁니다.

## 진행 상황

| 단계 | 내용 | 상태 |
|---|---|---|
| 1 | Win32 창 + D3D11 디바이스·스왑체인·백버퍼 뷰 | 완료 |
| 2 | 정점·인덱스 버퍼, HLSL 셰이더 컴파일과 바인딩 | 예정 |
| 3 | WIC 텍스처 로딩, 샘플러, 상수 버퍼 | 예정 |
| 4 | 스프라이트 N개 — 개별 드로우 콜 대 배칭 | 예정 |
| 5 | 계측 레이어와 비교 실험 | 예정 |

각 단계에서 무엇을 왜 그렇게 했는지는 `Docs/작업일지/`에 남깁니다.

## 구조

```
src/
  main.cpp       진입점. 창과 렌더러를 연결하고 게임 루프를 돌린다.
  Window.*       Win32 창 하나. D3D를 모른다. 크기 변경은 콜백으로만 알린다.
  Renderer.*     디바이스·스왑체인·백버퍼 뷰를 소유하고 한 프레임을 그린다.
  FrameTimer.h   QueryPerformanceCounter 기반 프레임 시간 측정.
  Common.h       COM 스마트 포인터 별칭과 HRESULT 검사 매크로.
Docs/            Obsidian 저장소. 단계별 작업일지와 개념 정리.
```
