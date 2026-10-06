#pragma once
#include <cstdint>
#include <cmath>
#include <cstring>
namespace theater_ui {
inline constexpr wchar_t pipe[]=L"\\\\.\\pipe\\EldenRingTheaterMode_1_17_Editor";
inline constexpr std::uint32_t magic=0x37495554;
// Version 2: recorder start/stop commands and recorder state in the snapshot (REC pill).
// Version 3: replay library page and open-by-index, since the launcher no longer has a library window.
// Version 4: named recordings (F5 asks the overlay for a name first), library sort/rename/delete,
//            richer library rows and a library message line. Request carries a UTF-8 text field.
// Host and overlay are built together; a version mismatch disconnects instead of guessing.
inline constexpr std::uint32_t version=4;
enum Command : std::uint32_t { poll, play, pause, stop, restart, seek, previous, next, speed, select, page, record_start, record_stop, replay_page, replay_open,
 record_named,    // text = display name; starts recording with that name
 replay_sort,     // value = key*2 + descending; key: 0 date, 1 size, 2 name, 3 duration
 replay_rename,   // value = library index, text = new display name
 replay_delete,   // value = library index; moved to the Recycle Bin with its sidecar files
 command_count };
// Mirrors erplay::RecordingState without making the overlay depend on the host headers.
enum RecordingState : std::uint32_t { record_idle, record_recording, record_paused, record_saving };
enum SortKey : std::uint32_t { sort_date, sort_size, sort_name, sort_duration, sort_key_count };
inline constexpr std::uint32_t replay_page_size=12;
inline constexpr std::size_t text_size=128;
struct Request { std::uint32_t magic_value{magic},version{theater_ui::version},command{};std::uint32_t reserved{};std::uint64_t sequence{},value{};char text[text_size]{}; };
struct Actor {std::uint64_t id{},handle{};std::uint32_t entity{},type{};std::int32_t npc{};std::uint32_t reserved{};};
// UTF-8 display name (metadata title, or the file name for older replays), truncated to fit.
// index is the position in the host's sorted list.
struct Replay {char name[96]{};std::uint64_t duration_ns{},bytes{},modified_unix{};std::uint32_t index{},loaded{};
 std::uint64_t recorded_unix{};char file[96]{};char game_version[16]{};char area[48]{};};
// Bounded page sizes are transport resource bounds, not actor or replay count limits.
struct Snapshot {std::uint32_t magic_value{magic},version{theater_ui::version},loaded{},active{},phase{},count{},offset{},total{};
 std::uint64_t sequence{},time_ns{},duration_ns{},selected{};double playback_speed{1};
 float live_position[3]{};std::uint32_t connected{},player_found{};char diagnostic[256]{};Actor actors[16]{};
 std::uint32_t recording_state{record_idle},recording_reserved{};std::uint64_t recording_ns{},recording_samples{};
 std::uint32_t replay_count{},replay_offset{},replay_total{},replay_reserved{};Replay replays[replay_page_size]{};
 // v4
 std::uint32_t sort_key{sort_date},sort_descending{1};
 std::uint32_t name_request{};   // increments when F5 asks the overlay for a recording name
 std::uint32_t library_message_id{}; std::uint32_t library_message_error{},v4_reserved{};
 char default_name[96]{};        // suggested name for the next recording
 char library_message[160]{};    // result of the last rename/delete/open, shown in the Replays panel
 char recording_name[96]{};      // display name of the recording in progress
};
static_assert(sizeof(Request)==32+text_size);
inline bool has_text(const Request&r){return r.text[0]!=0&&memchr(r.text,0,text_size)!=nullptr;}
inline bool valid(const Request&r,std::uint64_t last){
 if(r.magic_value!=magic||r.version!=version||r.reserved||!r.sequence||r.sequence<=last||r.command>=command_count)return false;
 if(!memchr(r.text,0,text_size))return false; // always NUL-terminated
 if(r.command==speed)return r.value==10||r.value==25||r.value==50||r.value==100||r.value==200||r.value==400;
 if(r.command==record_named)return r.value==0&&has_text(r);
 if(r.command==replay_rename)return has_text(r);
 if(r.command==replay_sort)return r.value<sort_key_count*2;
 return r.command==seek||r.command==select||r.command==page||r.command==replay_page||r.command==replay_open||r.command==replay_delete||(r.value==0&&!r.text[0]);
}
}
