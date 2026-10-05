#pragma once
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <stop_token>

namespace playback_worker {
// Dedicated producer, not a window timer. Never calls Win32 UI functions.
inline void run(std::stop_token stop, const std::function<void()>& tick) {
    HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
    if(!timer)timer=CreateWaitableTimerExW(nullptr,nullptr,0,TIMER_ALL_ACCESS);
    using Clock=std::chrono::steady_clock;
    constexpr auto period=std::chrono::nanoseconds(8'333'333); // 120 Hz request, measured separately.
    auto next=Clock::now();
    while(!stop.stop_requested()) {
        tick();next+=period;const auto now=Clock::now();
        if(next<=now)next=now+period; // Skip missed deadlines, no catch-up burst.
        if(timer){LARGE_INTEGER due{};due.QuadPart=-std::max<LONGLONG>(1,std::chrono::duration_cast<std::chrono::nanoseconds>(next-now).count()/100);
            if(SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(timer,100);
            else Sleep(1);
        }else Sleep(1);
    }
    if(timer)CloseHandle(timer);
}
}
