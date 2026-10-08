#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace theater_particle {

inline constexpr std::uint32_t protocol_version = 1;
inline constexpr std::uint32_t magic = 0x54525050; // "PPRT"

enum class CommandKind : std::uint32_t { spawn = 1, update = 2, remove = 3, clear = 4 };
enum class EffectKind : std::uint32_t { preset = 1, sp_effect = 2, ffx_id = 3 };

struct Command {
    std::uint32_t magic_value{magic};
    std::uint32_t version{protocol_version};
    CommandKind kind{CommandKind::spawn};
    EffectKind effect_kind{EffectKind::preset};
    std::uint64_t sequence{};
    std::uint64_t replay_time_ns{};
    std::uint64_t emitter_id{};
    std::uint32_t effect_id{};
    std::uint32_t reserved{};
    std::array<float, 3> position{};
    std::array<float, 4> rotation{0.f, 0.f, 0.f, 1.f};
    std::array<float, 3> scale{1.f, 1.f, 1.f};
    float intensity{1.f};
    float duration_seconds{1.f};
    float repeat_seconds{};
};

inline bool finite(const Command& c) {
    for (const auto v : c.position) if (!std::isfinite(v)) return false;
    for (const auto v : c.rotation) if (!std::isfinite(v)) return false;
    for (const auto v : c.scale) if (!std::isfinite(v)) return false;
    return std::isfinite(c.intensity) && std::isfinite(c.duration_seconds) && std::isfinite(c.repeat_seconds);
}

inline bool valid(const Command& c, std::uint64_t last_sequence = 0) {
    if (c.magic_value != magic || c.version != protocol_version || c.reserved || c.sequence <= last_sequence || !finite(c)) return false;
    if (c.kind == CommandKind::clear) return c.effect_id == 0;
    if (!c.emitter_id) return false;
    if (c.kind != CommandKind::spawn && c.kind != CommandKind::update && c.kind != CommandKind::remove) return false;
    if (c.kind == CommandKind::remove) return true;
    if (c.effect_kind != EffectKind::preset && c.effect_kind != EffectKind::sp_effect && c.effect_kind != EffectKind::ffx_id) return false;
    // Bound only by the nanosecond timestamp representation, not session length.
    constexpr double max_seconds = double(std::numeric_limits<std::uint64_t>::max()) / 1e9;
    if (double(c.duration_seconds) + double(c.repeat_seconds) >= max_seconds) return false;
    if (!c.effect_id || c.scale[0] <= 0.f || c.scale[1] <= 0.f || c.scale[2] <= 0.f || c.intensity < 0.f || c.duration_seconds < 0.f || c.repeat_seconds < 0.f) return false;
    const auto q2 = c.rotation[0] * c.rotation[0] + c.rotation[1] * c.rotation[1] + c.rotation[2] * c.rotation[2] + c.rotation[3] * c.rotation[3];
    return q2 > 0.99f && q2 < 1.01f;
}

} // namespace theater_particle
