// 정점 셰이더 (Vertex Shader)
//
// 정점 하나당 한 번 실행된다. 삼각형 하나면 세 번.
// 하는 일은 "이 정점이 화면 어디에 놓이는가"를 정하는 것.

// C++이 매 프레임 채워 넣는 값. Renderer.h의 SpriteConstants와 짝이다.
//
// 짝을 맞추는 것은 이름이 아니라 바이트 오프셋이다. register(b0)의 0이
// C++ VSSetConstantBuffers(0, ...)의 0과 만나고, 그 안쪽은 순수하게 바이트다.
// 이름이 달라도 돌아가고, 순서가 어긋나면 이름이 같아도 값이 뒤바뀐다.
//
// 하나가 16바이트 경계를 걸치면 다음 경계로 밀려나므로 float2끼리 짝지어 채웠다.
// 여기가 어긋나도 아무도 경고해주지 않는다. 값만 조용히 틀리게 나온다.
cbuffer SpriteConstants : register(b0)
{
    float2 screenSize;    //  0 ~  7   창 크기 (픽셀)
    float2 spritePos;     //  8 ~ 15   스프라이트 좌상단 위치 (픽셀)
    float2 spriteSize;    // 16 ~ 23   스프라이트 크기 (픽셀)
    float2 padding;       // 24 ~ 31   버퍼 전체를 16의 배수로 맞추는 용도
};

// IA(Input Assembler)가 정점 버퍼에서 꺼내 넣어주는 값.
// 뒤에 붙은 POSITION, COLOR가 시맨틱(semantic) 이름이다.
// C++ 쪽 입력 레이아웃에 적은 이름과 글자 하나까지 같아야 한다.
struct VSInput
{
    // 좌표가 아니라 "스프라이트 안에서의 비율"이다. 좌상단 (0,0), 우하단 (1,1).
    float2 position : POSITION;
    float4 color    : COLOR;
};

// 다음 단계(래스터라이저 -> 픽셀 셰이더)로 넘길 값.
struct VSOutput
{
    // SV_ 접두사는 System Value의 약자다. GPU가 특별하게 다루는 값이라는 뜻.
    // SV_Position은 "이게 최종 화면 좌표다"라고 알리는 약속된 이름이고,
    // 래스터라이저가 이 값을 보고 삼각형을 픽셀로 쪼갠다. 반드시 있어야 한다.
    float4 position : SV_Position;
    float4 color    : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // 비율(0~1) -> 화면 픽셀 좌표 -> NDC. 세 단계다.
    //
    // 계산을 여기서 하는 덕분에 정점 버퍼는 IMMUTABLE로 남는다. 스프라이트를 옮길 때
    // 정점을 다시 쓰지 않고 상수 버퍼 32바이트만 갱신하면 된다.
    float2 screenPos = spritePos + input.position * spriteSize;   // ① 비율 -> 픽셀
    float2 ndc       = screenPos / screenSize;                    // ② 픽셀 -> 0~1

    // ③ 0~1 -> NDC. x와 y는 공식이 다르다.
    // 화면 좌표는 아래가 +y인데 NDC는 위가 +y라, y만 부호를 뒤집는다.
    ndc.x = ndc.x * 2.0f - 1.0f;
    ndc.y = 1.0f - ndc.y * 2.0f;

    // z는 0(가장 앞), w는 1.
    // 래스터라이저가 SV_Position을 w로 나누므로(원근 나눗셈) 0을 넣으면 좌표가 NaN이 되고
    // 삼각형이 통째로 버려진다. 2D라 원근이 필요 없어서 1로 나눠 그대로 통과시킨다.
    output.position = float4(ndc, 0.0f, 1.0f);

    // SV_가 안 붙은 값은 래스터라이저의 보간을 타고 픽셀 셰이더로 간다.
    output.color = input.color;

    return output;
}
