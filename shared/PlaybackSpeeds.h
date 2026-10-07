#pragma once
#include <array>
#include <cstdint>
namespace theater_speed {
inline constexpr std::array<std::uint64_t, 10> presets{10, 20, 25, 50, 75, 100, 125, 150, 200, 400};
inline constexpr std::array<const char*, 10> labels{"0.10x", "0.20x", "0.25x", "0.50x", "0.75x", "1.00x", "1.25x", "1.50x", "2.00x", "4.00x"};
inline bool valid(std::uint64_t centi) {
    for (auto value : presets) if (centi == value) return true;
    return false;
}
inline bool valid(double speed) {
    for (auto value : presets) if (speed == double(value) / 100.) return true;
    return false;
}
}
