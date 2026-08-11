#pragma once

#include "Common.h"

// 프레임 시간을 재는 최소한의 타이머.
//
// 지금은 창 제목에 FPS를 띄우는 용도지만, 이 프로젝트의 목적이 "드로우 콜 방식에 따라
// 성능이 어떻게 달라지는가를 실측하는 것"이므로 계측은 처음부터 코드 안에 둔다.
// 나중에 프레임 시간 분포와 드로우 콜 수를 함께 기록하는 자리로 확장한다.
class FrameTimer
{
public:
    FrameTimer()
    {
        // QueryPerformanceFrequency가 돌려주는 값은 부팅 후 바뀌지 않는다.
        // 그래서 매번 부르지 않고 한 번만 받아 둔다.
        QueryPerformanceFrequency(&m_frequency);
        QueryPerformanceCounter(&m_lastTick);
        m_windowStartTick = m_lastTick;
    }

    // 매 프레임 한 번 부른다. 직전 프레임에 걸린 시간(초)을 돌려준다.
    double Tick()
    {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);

        const double delta = static_cast<double>(now.QuadPart - m_lastTick.QuadPart)
                           / static_cast<double>(m_frequency.QuadPart);
        m_lastTick = now;

        ++m_framesInWindow;
        return delta;
    }

    // 마지막 집계 이후 1초가 지났으면 평균 FPS를 outFps에 담고 true를 돌려준다.
    //
    // 매 프레임의 순간값을 쓰지 않는 이유: 순간값은 심하게 튀어서 눈으로 비교할 수 없다.
    // 일정 구간의 평균이라야 "이 방식이 저 방식보다 빠르다"를 말할 수 있다.
    bool TryGetAverageFps(double& outFps)
    {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);

        const double elapsed = static_cast<double>(now.QuadPart - m_windowStartTick.QuadPart)
                             / static_cast<double>(m_frequency.QuadPart);

        if (elapsed < 1.0)
            return false;

        outFps = m_framesInWindow / elapsed;

        m_framesInWindow  = 0;
        m_windowStartTick = now;
        return true;
    }

private:
    LARGE_INTEGER m_frequency{};
    LARGE_INTEGER m_lastTick{};
    LARGE_INTEGER m_windowStartTick{};
    int           m_framesInWindow = 0;
};
