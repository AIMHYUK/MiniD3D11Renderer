// 정점 셰이더 (Vertex Shader)
//
// 정점 하나당 한 번 실행된다. 삼각형 하나면 세 번.
// 하는 일은 "이 정점이 화면 어디에 놓이는가"를 정하는 것.

// IA(Input Assembler)가 정점 버퍼에서 꺼내 넣어주는 값.
// 뒤에 붙은 POSITION, COLOR가 시맨틱(semantic) 이름이다.
// C++ 쪽 입력 레이아웃에 적은 이름과 글자 하나까지 같아야 한다.
struct VSInput
{
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

    // 지금은 좌표를 그대로 통과시킨다.
    // C++에서 이미 NDC(-1 ~ +1) 값으로 넣어줬기 때문이다.
    // z는 0(가장 앞), w는 1(원근 나눗셈을 하지 않겠다는 뜻).
    output.position = float4(input.position, 0.0f, 1.0f);

    output.color = input.color;
    return output;
}
