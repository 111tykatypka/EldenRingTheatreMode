#pragma once
#include <cstdint>
#include <cmath>
namespace theater_ui {
inline constexpr wchar_t pipe[]=L"\\\\.\\pipe\\EldenRingTheaterMode_1_17_Editor";
inline constexpr std::uint32_t magic=0x37495554;
// Version 2: recorder start/stop commands and recorder state in the snapshot (REC pill).
// Version 3: replay library page and open-by-index, since the launcher no longer has a library window.
// Host and overlay are built together; a version mismatch disconnects instead of guessing.
inline constexpr std::uint32_t version=3;
enum Command : std::uint32_t { poll, play, pause, stop, restart, seek, previous, next, speed, select, page, record_start, record_stop, replay_page, replay_open };
// Mirrors erplay::RecordingState without making the overlay depend on the host headers.
enum RecordingState : std::uint32_t { record_idle, record_recording, record_paused, record_saving };
inline constexpr std::uint32_t replay_page_size=12;
struct Request { std::uint32_t magic_value{magic},version{theater_ui::version},command{};std::uint32_t reserved{};std::uint64_t sequence{},value{}; };
struct Actor {std::uint64_t id{},handle{};std::uint32_t entity{},type{};std::int32_t npc{};std::uint32_t reserved{};};
// UTF-8 file name without extension, truncated to fit. index is the position in the host's newest-first list.
struct Replay {char name[96]{};std::uint64_t duration_ns{},bytes{},modified_unix{};std::uint32_t index{},loaded{};};
// Bounded page sizes are transport resource bounds, not actor or replay count limits.
struct Snapshot {std::uint32_t magic_value{magic},version{theater_ui::version},loaded{},active{},phase{},count{},offset{},total{};
 std::uint64_t sequence{},time_ns{},duration_ns{},selected{};double playback_speed{1};
 float live_position[3]{};std::uint32_t connected{},player_found{};char diagnostic[256]{};Actor actors[16]{};
 std::uint32_t recording_state{record_idle},recording_reserved{};std::uint64_t recording_ns{},recording_samples{};
 std::uint32_t replay_count{},replay_offset{},replay_total{},replay_reserved{};Replay replays[replay_page_size]{};};
static_assert(sizeof(Request)==32);
inline bool valid(const Request&r,std::uint64_t last){
 if(r.magic_value!=magic||r.version!=version||r.reserved||!r.sequence||r.sequence<=last||r.command>replay_open)return false;
 if(r.command==speed)return r.value==10||r.value==25||r.value==50||r.value==100||r.value==200||r.value==400;
 return r.command==seek||r.command==select||r.command==page||r.command==replay_page||r.command==replay_open||r.value==0;
}
}
