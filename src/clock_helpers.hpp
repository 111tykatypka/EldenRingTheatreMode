#pragma once
#include <windows.h>
#include <cstdint>
#ifdef _MSC_VER
#pragma comment(lib,"Mincore.lib")
#endif
namespace theater_clock {
inline std::uint64_t monotonic_ns(){ULONGLONG ticks{};QueryInterruptTimePrecise(&ticks);return ticks*100ULL;}
}
