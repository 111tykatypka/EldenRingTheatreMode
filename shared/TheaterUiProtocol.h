#pragma once
#include <cstdint>
#include <cmath>
namespace theater_ui {
inline constexpr wchar_t pipe[]=L"\\\\.\\pipe\\EldenRingTheaterMode_1_17_Editor";
inline constexpr std::uint32_t magic=0x37495554;
// Version 2: recorder start/stop commands and recorder state in the snapshot (REC pill).
// Host and overlay are built together; a version mismatch disconnects instead of guessing.
inline constexpr std::uint32_t version=2;
enum Command : std::uint32_t { poll, play, pause, stop, restart, seek, previous, next, speed, select, page, record_start, record_stop };
// Mirrors erplay::RecordingState without making the overlay depend on the host headers.
enum RecordingState : std::uint32_t { record_idle, record_recording, record_paused, record_saving };
struct Request { std::uint32_t magic_value{magic},version{theater_ui::version},command{};std::uint32_t reserved{};std::uint64_t sequence{},value{}; };
struct Actor {std::uint64_t id{},handle{};std::uint32_t entity{},type{};std::int32_t npc{};std::uint32_t reserved{};};
// Bounded page size is a transport resource bound, not an actor-count limit.
struct Snapshot {std::uint32_t magic_value{magic},version{theater_ui::version},loaded{},active{},phase{},count{},offset{},total{};
 std::uint64_t sequence{},time_ns{},duration_ns{},selected{};double playback_speed{1};
 float live_position[3]{};std::uint32_t connected{},player_found{};char diagnostic[256]{};Actor actors[16]{};
 std::uint32_t recording_state{record_idle},recording_reserved{};std::uint64_t recording_ns{},recording_samples{};};
static_assert(sizeof(Request)==32);
inline bool valid(const Request&r,std::uint64_t last){
 if(r.magic_value!=magic||r.version!=version||r.reserved||!r.sequence||r.sequence<=last||r.command>record_stop)return false;
 if(r.command==speed)return r.value==10||r.value==25||r.value==50||r.value==100||r.value==200||r.value==400;
 return r.command==seek||r.command==select||r.command==page||r.value==0;
}
}
