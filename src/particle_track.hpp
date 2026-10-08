#pragma once

#include "ParticleProtocol.h"
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace particle_track {

struct Event {
    theater_particle::Command command{};
    bool loop = true;
};

// Host-side scheduler. It produces immutable commands; the game adapter still owns
// all native object access and may reject commands when CSSfx is unavailable.
class Scheduler {
public:
    bool add(Event event);
    bool remove(std::uint64_t emitter_id);
    void clear();
    void seek(std::uint64_t replay_time_ns);
    std::vector<theater_particle::Command> evaluate(std::uint64_t replay_time_ns);
    std::size_t size() const noexcept { return events_.size(); }

private:
    std::vector<Event> events_;
    std::uint64_t last_time_ns_{};
    std::uint64_t sequence_{};
    bool rebuild_ = true;
    std::unordered_map<std::uint64_t, std::uint64_t> active_cycles_;
};

} // namespace particle_track
