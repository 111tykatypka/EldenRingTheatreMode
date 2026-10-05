#pragma once
#include "player_action.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <stdexcept>
namespace erplay {
// Typed track 3, schema 1. Presence means observed in the capture set, NOT
// spawn/death.
enum class CharacterKind : std::uint16_t {
  registry = 1,
  transform = 2,
  presence = 3
};
struct CharacterRecord {
  std::uint16_t version{1};
  CharacterKind kind{CharacterKind::transform};
  std::uint32_t flags{};
  std::uint64_t id{}, timestamp_ns{}, native_handle{};
  std::uint32_t entity_id{};
  std::int32_t npc_param{};
  std::int32_t block_id{};
  std::uint32_t character_type{};
  std::array<float, 3> position{};
  std::array<float, 4> orientation{0, 0, 0, 1};
  ActionState action;
  std::uint32_t reserved{};
};
inline constexpr std::uint32_t character_record_bytes = 112;
CharacterRecord read_character_record(std::istream &);
void write_character_record(std::ostream &, const CharacterRecord &);
struct CharacterInfo {
  CharacterRecord registry;
  std::uint64_t first_ns{}, last_ns{}, samples{}, observations{};
};
class CharacterValidator {
public:
  std::map<std::uint64_t, CharacterInfo> actors;
  std::uint64_t records{}, samples{}, latest{};
  void accept(const CharacterRecord &r) {
    if (r.version != 1 || r.reserved || !r.id || r.flags > 1)
      throw std::runtime_error("invalid character schema/identity/flags");
    if (r.kind == CharacterKind::registry) {
      if (actors.contains(r.id))
        throw std::runtime_error("duplicate character registry");
      actors.emplace(r.id, CharacterInfo{r, r.timestamp_ns, r.timestamp_ns});
    } else {
      auto it = actors.find(r.id);
      if (it == actors.end())
        throw std::runtime_error("unregistered character");
      auto &a = it->second;
      if (r.timestamp_ns < a.last_ns)
        throw std::runtime_error("character timestamp regression");
      if (r.kind == CharacterKind::transform) {
        double norm = 0;
        for (float f : r.position)
          if (!std::isfinite(f))
            throw std::runtime_error("non-finite character position");
        for (float f : r.orientation) {
          if (!std::isfinite(f))
            throw std::runtime_error("non-finite character quaternion");
          norm += double(f) * f;
        }
        if (norm < .25 || norm > 2.25 || !r.action.valid())
          throw std::runtime_error("invalid character orientation/action");
        ++a.samples;
        ++samples;
        if (r.action.flags)
          ++a.observations;
      } else if (r.kind != CharacterKind::presence)
        throw std::runtime_error("unknown character record kind");
      a.last_ns = r.timestamp_ns;
    }
    ++records;
    latest = std::max(latest, r.timestamp_ns);
  }
};
} // namespace erplay
