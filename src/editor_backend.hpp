#pragma once
#include <windows.h>

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include "character_client.hpp"
#include "erplay.hpp"
#include "game_control.hpp"
#include "replay_player.hpp"

#include "GameProfile.h"
#include "clock_helpers.hpp"
#include "game_launcher.hpp"
#include "in_game_replay.hpp"
#include "playback_worker.hpp"
#include <shlobj.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <functional>
namespace fs = std::filesystem;
namespace theater {
enum class Command { start, pause, resume, stop };
struct ReplayEntry {
  fs::path path;
  erplay::Summary summary;
  std::uintmax_t bytes;
  fs::file_time_type modified;
};
struct Snapshot {
  bool connected{}, player{};
  erplay::RecordingState state{erplay::RecordingState::idle};
  std::uint64_t samples{}, dropped{}, reconnects{}, bytes{}, active_ns{};
  std::uint64_t capture_frames{},capture_drops{};
  std::uint16_t sample_protocol{};
  erplay::ActionState action;
  double rate{}, latency_ms{};
  std::vector<ReplayEntry> replays;
  std::wstring error;
};
struct App {
  HWND window{};
  bool stop_hotkey{};
  character_capture::Client characters;
  game_launcher::Launcher launcher;
  fs::path loader_path, game_path;
  std::atomic_bool show_debug_console{}; // launcher checkbox; YAFSML console window
  // Named recordings: F5 asks the overlay for a name (name_request) when it is connected;
  // the next START uses pending_record_name (display name + file name), then clears it.
  std::mutex names_mutex;
  std::string pending_record_name, recording_name;
  std::string recording_path; // UTF-8 final .erplay path of the recording in progress; the game writes <path>.bones beside it
  std::atomic<std::uint32_t> name_request{};
  // Replay Library sort (persisted in Library.settings) and the last library message for the overlay.
  std::atomic<std::uint32_t> sort_key{0};
  std::atomic_bool sort_descending{true};
  std::string library_message;
  std::uint32_t library_message_id{}, library_message_error{};
  std::atomic_bool sample_pipe_ready{};
  game_control::Client control;
  std::unique_ptr<in_game_replay::Controller> game_replay;
  std::recursive_mutex replay_mutex;
  std::jthread playback_thread;
  double clock_hz{};
  std::atomic<DWORD> game_pid{};
  fs::path root, replays, logs;
  std::mutex mutex, commands_mutex;
  Snapshot data;
  std::queue<Command> commands;
  std::atomic_bool stopping{};
  std::jthread worker;
  std::ofstream log;
  std::unique_ptr<replay::Player> replay_player;
  // UI-owned document data; replay worker only touches replay_player under
  // replay_mutex.
  std::vector<std::uint64_t> replay_bookmarks;
  struct CharacterView {
    erplay::CharacterInfo info;
    std::vector<erplay::CharacterRecord> trajectory;
    std::optional<erplay::CharacterRecord> current;
  };
  std::vector<CharacterView> character_views;
  std::vector<erplay::Vec3> preview_path;
  fs::path opened_replay;
  std::uint64_t limit_ns{};
  bool animation{};bool xz_diagnostic{};
  bool actor_playback{};
  bool selected_actor_only{};std::uint64_t selected_replay_actor{};
};
extern App app;
struct PlaybackView {
  bool loaded{}, active{};
  replay::State state;
  erplay::Summary summary;
  in_game_replay::Phase phase{};
  std::wstring diagnostic;
  double clock_hz{};
};
PlaybackView playback_view();
Snapshot recorder_view();
void refresh_character_cursor(std::uint64_t);
void initialize(HWND);
void shutdown();
void post_command(Command);
bool handle_global_hotkey(UINT);
void emergency_stop();
void play_replay();
void return_replay_start();
void pause_replay();
void toggle_replay();
void restart_replay();
void seek_replay(std::uint64_t);
void step_replay(int);
void open_replay(const fs::path &);
void unload_replay();
void refresh_library();
void bookmark_add();
void bookmark_delete(std::uint64_t);
void launch_game();
void choose_loader();
void choose_game();
void log_line(const std::string &);
std::wstring time_text(std::uint64_t);
fs::path executable_directory();
} // namespace theater
