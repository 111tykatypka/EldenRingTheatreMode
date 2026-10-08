#include "particle_track.hpp"
#include <algorithm>

namespace particle_track {
bool Scheduler::add(Event event) {
    if (event.command.kind != theater_particle::CommandKind::spawn || !theater_particle::valid(event.command)) return false;
    if (std::any_of(events_.begin(), events_.end(), [&](const Event& e) { return e.command.emitter_id == event.command.emitter_id; })) return false;
    events_.push_back(event); return true;
}
bool Scheduler::remove(std::uint64_t id) {
    const auto before = events_.size();
    std::erase_if(events_, [id](const Event& e) { return e.command.emitter_id == id; });
    return before != events_.size();
}
void Scheduler::clear() { events_.clear(); rebuild_ = true; }
void Scheduler::seek(std::uint64_t t) { last_time_ns_ = t; rebuild_ = true; }

std::vector<theater_particle::Command> Scheduler::evaluate(std::uint64_t t) {
    using namespace theater_particle;
    std::vector<Command> out;
    auto emit = [&](Command c, CommandKind kind) {
        c.kind = kind; c.sequence = ++sequence_; c.replay_time_ns = t; out.push_back(c);
    };
    if (rebuild_ || t < last_time_ns_) {
        emit(Command{}, CommandKind::clear); active_cycles_.clear(); rebuild_ = false;
    }
    std::unordered_map<std::uint64_t, std::uint64_t> desired;
    for (const auto& event : events_) {
        const auto& c = event.command;
        if (c.replay_time_ns > t) continue;
        const auto duration = static_cast<std::uint64_t>(double(c.duration_seconds) * 1e9);
        const auto age = t - c.replay_time_ns;
        const auto period = static_cast<std::uint64_t>((double(c.duration_seconds) + double(c.repeat_seconds)) * 1e9);
        const auto cycle = duration && event.loop && period ? age / period : 0;
        const bool alive = !duration || (event.loop && period ? age % period : age) < duration;
        if (!alive) continue;
        desired.emplace(c.emitter_id, cycle);
        const auto active = active_cycles_.find(c.emitter_id);
        if (active == active_cycles_.end() || active->second != cycle) {
            if (active != active_cycles_.end()) emit(c, CommandKind::remove);
            emit(c, CommandKind::spawn);
        }
    }
    for (const auto& [id, cycle] : active_cycles_) {
        if (!desired.contains(id)) { Command c; c.emitter_id = id; emit(c, CommandKind::remove); }
    }
    active_cycles_ = std::move(desired); last_time_ns_ = t; return out;
}
} // namespace particle_track
