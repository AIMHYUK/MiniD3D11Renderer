// 픽셀 셰이더 (Pixel Shader)
//
// 삼각형이 덮은 픽셀 하나당 한 번 실행된다.
// 화면을 반쯤 채우는 삼각형이면 수십만 번 실행된다.
// 정점 셰이더가 3번 도는 동안 이쪽은 수십만 번 도는 셈이라,
// 무거운 계산을 어디에 두느냐가 성능을 가른다.

struct PSInput
{
    float4 position : SV_Position;
    float4 color    : COLOR;
};

// 반환값의 SV_Target은 "이 색을 렌더 타깃에 쓴다"는 뜻이다.
// OM(Output Merger)이 이 값을 받아 백버퍼에 기록한다.
float4 main(PSInput input) : SV_Target
{
    // 여기서 받는 color는 정점에 넣은 색 그대로가 아니다.
    // 래스터라이저가 세 정점의 값을 픽셀 위치에 맞춰 자동으로 섞어(보간해) 준다.
    // 삼각형 가운데가 세 색이 뒤섞인 그라데이션으로 나오는 이유다.
    return input.color;
}
