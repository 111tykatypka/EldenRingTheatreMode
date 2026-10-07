#include "ingame_editor_server.hpp"
#include "editor_backend.hpp"
#include "TheaterHotkeys.h"
#include "replay_library.hpp"
namespace theater {
App app;
constexpr UINT WM_REFRESH = WM_APP + 1;
constexpr std::uint32_t MAGIC = 0x544D5354;
struct WireMessage {
  std::uint32_t magic;
  std::uint16_t version, kind;
  std::uint64_t sequence, timestamp_ns;
  float position[3], quaternion_xyzw[4], euler_raw[3];
  std::uint32_t player_present, reserved;
  erplay::ActionState action;
  erplay::CaptureFrame capture;
};
static_assert(sizeof(WireMessage) == 104 + erplay::capture_wire_bytes &&
              offsetof(WireMessage, action) == 72);
PlaybackView playback_view() {
  std::lock_guard lock(app.replay_mutex);
  PlaybackView v;
  v.loaded = bool(app.replay_player);
  // The player character plays bone replays inside the game (adapter bone_replay); the host only
  // owns the timeline clock, so "active" (the retired position-only in-game replay) is always false.
  v.clock_hz = app.clock_hz;
  if (v.loaded) {
    v.state = app.replay_player->state();
    v.summary = app.replay_player->summary();
    v.phase = v.state.status == replay::Status::playing ? 2u : v.state.status == replay::Status::paused ? 3u : 0u;
  }
  return v;
}
std::uint64_t monotonic_ns() { return theater_clock::monotonic_ns(); }
std::uint64_t unix_ns() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count());
}
fs::path app_root() {
  wchar_t b[32768]{};
  DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", b, 32768);
  return (n && n < 32768 ? fs::path(b) : fs::temp_directory_path()) /
         L"EldenRingTheaterMode";
}
std::wstring time_text(std::uint64_t ns) {
  auto ms = ns / 1'000'000ULL;
  std::wostringstream o;
  o << std::setfill(L'0') << std::setw(2) << (ms / 3'600'000ULL) << L":"
    << std::setw(2) << ((ms / 60'000ULL) % 60) << L":" << std::setw(2)
    << ((ms / 1000ULL) % 60) << L"." << std::setw(3) << (ms % 1000);
  return o.str();
}
std::wstring file_stem() {
  std::time_t now = std::time(nullptr);
  std::tm t{};
  localtime_s(&t, &now);
  std::wostringstream s;
  s << L"replay_" << std::put_time(&t, L"%Y-%m-%d_%H%M%S");
  return s.str();
}
void log_line(const std::string &s) {
  std::lock_guard lock(app.mutex);
  if (app.log)
    app.log << s << "\n" << std::flush;
}
void playback_tick() {
  static auto perf_start = replay::Player::Clock::now();
  static std::uint64_t generated = 0;
  std::lock_guard lock(app.replay_mutex);
  try {
    if (app.replay_player) {
      app.replay_player->advance();
      ++generated;
    }
  } catch (const std::exception &e) {
    log_line(std::string("REPLAY_WORKER_ERROR=") + e.what());
  }
  const auto now = replay::Player::Clock::now();
  const double elapsed =
      std::chrono::duration<double>(now - perf_start).count();
  if (elapsed >= 1.0) {
    app.clock_hz = double(generated) / elapsed;
    if (generated)
      log_line(
          "REPLAY_PERF host_clock_hz=" + std::to_string(app.clock_hz) +
          " ipc_send_hz=" + std::to_string(app.control.state().replay_send_hz));
    generated = 0;
    perf_start = now;
  }
}
bool handle_global_hotkey(UINT id) {
  if(id<1||id>2)return false;
  log_line("GLOBAL_HOTKEY received id="+std::to_string(id));
  if(id==2)emergency_stop();
  else if(ingame_editor::overlay_connected()){
    // The overlay shows a "Name this replay" box; it sends record_named to start.
    ++app.name_request;log_line("RECORD name requested from the overlay");
  } else post_command(Command::start); // no overlay: start with the default name
  return true;
}
void post_command(Command c) {
  if ((c == Command::start || c == Command::resume) && playback_view().active) {
    log_line("Recording refused while in-game replay is active; press STOP / "
             "F6 first");
    {std::lock_guard lock(app.mutex);app.data.error=L"Recording blocked: stop replay with F6 first.";}
    return;
  }
  {
    std::lock_guard lock(app.commands_mutex);
    app.commands.push(c);
  }
  const char* name=c==Command::start?"START":c==Command::pause?"PAUSE":c==Command::resume?"RESUME":"STOP";
  log_line(std::string("RECORD_COMMAND queued ")+name);
  PostMessageW(app.window, WM_REFRESH, 0, 0);
}
bool read_message(HANDLE h, WireMessage &m) {
  auto read = [&](char *p, std::size_t left) {
    while (left && !app.stopping.load()) {
      DWORD n = 0;
      if (!ReadFile(h, p, static_cast<DWORD>(left), &n, nullptr) || !n)
        return false;
      p += n;
      left -= n;
    }
    return left == 0;
  };
  if (!read(reinterpret_cast<char *>(&m), 72))
    return false;
  if (m.magic != MAGIC || (m.version != 1 && m.version != 2 && m.version != 3))
    return false;
  if(m.version==1)return true;
  if((m.version==2&&m.reserved!=0)||(m.version==3&&m.reserved!=erplay::capture_wire_bytes))return false;
  if(!read(reinterpret_cast<char*>(&m.action),32))return false;
  return m.version==2||(read(reinterpret_cast<char*>(&m.capture),erplay::capture_wire_bytes)&&erplay::capture_frame_valid(m.capture));
}
std::vector<ReplayEntry> scan_replays() {
  std::vector<ReplayEntry> out;
  std::error_code ec;
  for (const auto &it : fs::directory_iterator(app.replays, ec)) {
    if (ec)
      break;
    if (!it.is_regular_file() || it.path().extension() != L".erplay")
      continue;
    try {
      out.push_back({it.path(), erplay::validate(it.path()), it.file_size(),
                     it.last_write_time()});
    } catch (const std::exception &e) {
      log_line(std::string("Replay validation failed: ") + e.what());
    }
  }
  library::sort(out, {static_cast<library::SortKey>(app.sort_key.load()), app.sort_descending.load()});
  return out;
}
void set_data(const Snapshot &s) {
  std::lock_guard lock(app.mutex);
  auto files = std::move(app.data.replays);
  app.data = s;
  app.data.replays = std::move(files);
}

std::queue<Command> take_commands() {
  std::lock_guard lock(app.commands_mutex);
  std::queue<Command> q;
  std::swap(q, app.commands);
  return q;
}
void refresh_window() { PostMessageW(app.window, WM_REFRESH, 0, 0); }
fs::path executable_directory() {
  wchar_t path[32768]{};
  const DWORD n = GetModuleFileNameW(nullptr, path, 32768);
  if (!n || n >= 32768)
    throw std::runtime_error("Cannot determine host executable directory");
  return fs::path(path).parent_path();
}
fs::path default_loader() {
  PWSTR desktop = nullptr;
  fs::path path;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &desktop))) {
    path = fs::path(desktop) / L"YAFSML-v0.10.4" / L"YAFSML.exe";
    CoTaskMemFree(desktop);
  }
  return path;
}
void load_loader_path() {
  app.game_path = TM_EXPECTED_EXE_PATH;
  std::ifstream game_config(app.root / L"Game.path", std::ios::binary);
  if(game_config){try{const std::string text{std::istreambuf_iterator<char>(game_config),std::istreambuf_iterator<char>()};if(!text.empty())app.game_path=game_launcher::wide(text);}catch(const std::exception&e){log_line(std::string("Cannot load game path: ")+e.what());}}
  app.loader_path = default_loader();
  std::ifstream input(app.root / L"YAFSML.path", std::ios::binary);
  if (input) {
    try {
      const std::string text{std::istreambuf_iterator<char>(input),
                             std::istreambuf_iterator<char>()};
      if (!text.empty())
        app.loader_path = fs::path(game_launcher::wide(text));
    } catch (const std::exception &e) {
      log_line(std::string("Cannot load YAFSML path: ") + e.what());
    }
  }
}
void choose_loader() {
  wchar_t name[32768]{};
  wcsncpy_s(name, app.loader_path.c_str(), _TRUNCATE);
  OPENFILENAMEW ofn{sizeof(ofn)};
  ofn.hwndOwner = app.window;
  ofn.lpstrFilter =
      L"YAFSML launcher (YAFSML.exe)\0YAFSML.exe\0Executable files\0*.exe\0";
  ofn.lpstrFile = name;
  ofn.nMaxFile = 32768;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&ofn))
    return;
  app.loader_path = name;
  try {
    std::ofstream output(app.root / L"YAFSML.path",
                         std::ios::binary | std::ios::trunc);
    const auto text = game_launcher::utf8(app.loader_path.wstring());
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output)
      throw std::runtime_error("Cannot save YAFSML path");
    log_line("YAFSML selected: " + text);
  } catch (const std::exception &e) {
    MessageBoxW(app.window, game_launcher::wide(e.what()).c_str(),
                L"Launcher settings", MB_ICONERROR | MB_OK);
  }
}
void choose_game() {
  if(app.game_pid.load() || app.launcher.state().busy())return;
  wchar_t name[32768]{};wcsncpy_s(name, app.game_path.c_str(), _TRUNCATE);
  OPENFILENAMEW ofn{sizeof(ofn)};ofn.hwndOwner=app.window;
  ofn.lpstrFilter=L"Elden Ring (eldenring.exe)\0eldenring.exe\0";
  ofn.lpstrFile=name;ofn.nMaxFile=32768;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
  if(!GetOpenFileNameW(&ofn))return;
  try{
    // The launcher and injected DLL both use tm_validate_profile. A selected
    // path cannot opt out of the full hash/version/architecture checks.
    std::ofstream output(app.root/L"Game.path",std::ios::binary|std::ios::trunc);
    const auto text=game_launcher::utf8(name);output.write(text.data(),static_cast<std::streamsize>(text.size()));
    output.flush();if(!output)throw std::runtime_error("Cannot save game path");
    app.game_path=name;log_line("Game executable selected: "+text);
  }catch(const std::exception&e){MessageBoxW(app.window,game_launcher::wide(e.what()).c_str(),L"Game settings",MB_ICONERROR|MB_OK);}
}
void emergency_stop() {
  std::lock_guard playback_lock(app.replay_mutex);
  app.control.emergency_stop();
  if (app.replay_player)
    app.replay_player->stop();
  post_command(Command::stop);
  log_line("Emergency STOP requested: probe/replay OFF, ReplayPlayer stopped, "
           "recorder stop requested");
}
void pause_replay() {
  std::lock_guard playback_lock(app.replay_mutex);
  if (app.replay_player)
    app.replay_player->pause();
}
void toggle_replay() {
  // Decide on the authoritative host state, not a delayed overlay snapshot.
  std::lock_guard playback_lock(app.replay_mutex);
  if (!app.replay_player) return;
  if (app.replay_player->state().status == replay::Status::playing)
    pause_replay();
  else
    play_replay();
  log_line("REPLAY_TOGGLE time_ns=" +
           std::to_string(app.replay_player->state().timestamp_ns) +
           " playing=" + std::to_string(app.replay_player->state().status == replay::Status::playing));
}
void seek_replay(std::uint64_t t) {
  std::lock_guard playback_lock(app.replay_mutex);
  if (!app.replay_player)
    return;
  app.replay_player->seek(t);
}
void step_replay(int direction) {
  std::lock_guard playback_lock(app.replay_mutex);
  if (!app.replay_player)
    return;
  app.replay_player->step(direction);
}
void save_bookmarks() {
  std::lock_guard playback_lock(app.replay_mutex);
  if (app.opened_replay.empty())
    return;
  auto path = app.opened_replay;
  path += L".bookmarks";
  try {
    replay::BookmarkStore store(path);
    for (auto t : app.replay_bookmarks)
      store.add(t);
    store.save();
  } catch (const std::exception &e) {
    log_line(std::string("Bookmark save failed: ") + e.what());
  }
}
void load_bookmarks() {
  std::lock_guard playback_lock(app.replay_mutex);
  app.replay_bookmarks.clear();
  auto path = app.opened_replay;
  path += L".bookmarks";
  try {
    replay::BookmarkStore store(path);
    store.load();
    app.replay_bookmarks = store.timestamps();
  } catch (const std::exception &e) {
    log_line(std::string("Bookmark load failed: ") + e.what());
  }
}
void build_preview_path() {
  std::lock_guard playback_lock(app.replay_mutex);
  app.preview_path.clear();
  if (!app.replay_player)
    return;
  const auto n = app.replay_player->summary().sample_count;
  const auto stride = std::max<std::uint64_t>(1, (n + 2999) / 3000);
  for (std::uint64_t i = 0; i < n; i += stride) {
    const auto s = app.replay_player->reader().sample(i);
    app.preview_path.push_back(s.position);
  }
  if (n && ((n - 1) % stride) != 0)
    app.preview_path.push_back(
        app.replay_player->reader().sample(n - 1).position);
}
bool register_hotkey(HWND w, int id, UINT key, const char *name) {
  if (RegisterHotKey(w, id, MOD_NOREPEAT, key)) {
    log_line(std::string("Global hotkey registered: ") + name);
    return true;
  }
  const auto e = GetLastError();
  log_line(std::string("Global hotkey registration failed: ") + name +
           " error=" + std::to_string(e));
  std::lock_guard lock(app.mutex);
  app.data.error = L"A global hotkey could not be registered.";
  return false;
}
void launch_game() {
  try {
    const game_launcher::Paths paths{app.loader_path, app.game_path,
                                     executable_directory() /
                                         L"TheaterMode.dll",
                                     app.root / L"launch" / L"YAFSML.ini", app.show_debug_console.load()};
    app.launcher.start(
        paths,
        [] {
          const auto control = app.control.state();
          return game_launcher::Runtime{
              app.sample_pipe_ready.load() && app.stop_hotkey,
              app.game_pid.load(), control.connected, control.ready};
        },
        log_line);
  } catch (const std::exception &e) {
    MessageBoxW(app.window, game_launcher::wide(e.what()).c_str(),
                L"Game launch failed", MB_ICONERROR | MB_OK);
  }
}
// A replay recorded with bone data has "<file>.world" (or the older "<file>.bones") beside it. The game DLL plays those bones on
// the player itself, following this host timeline, so the old position-only in-game replay is skipped.
bool has_bones(const fs::path& replay){if(replay.empty())return false;std::error_code ec;for(const wchar_t* ext:{L".world",L".bones"}){auto p=replay;p+=ext;if(fs::is_regular_file(p,ec))return true;}return false;}
void play_replay() {
  std::lock_guard playback_lock(app.replay_mutex);
  if (!app.replay_player)
    return;
  // Replays without bone data (recorded before bone replays) only move the timeline.
  if (!has_bones(app.opened_replay)) log_line("REPLAY_PLAY timeline only: this replay has no bone data");
  app.replay_player->play();
}
void restart_replay() {
  std::lock_guard playback_lock(app.replay_mutex);
  if (!app.replay_player)
    return;
  app.replay_player->restart();
}
void pipe_worker() {
  fs::path log_path = app.logs / L"TheaterModeRecorder.log";
  {
    std::lock_guard lock(app.mutex);
    if (!app.log.is_open())
      app.log.open(log_path, std::ios::binary | std::ios::app);
  }
  std::map<std::uint64_t, bool> character_known;
  std::map<std::uint64_t, erplay::CharacterRecord> character_last_written;
  std::uint64_t character_samples = 0, character_min_source = 0;
  std::unique_ptr<erplay::Writer> writer;
  std::unique_ptr<erplay::RecordingSession> session;
  Snapshot snap;
  std::uint64_t last_seq = 0, last_source = 0, first_source = 0,
                window_start = GetTickCount64(), window_samples = 0,
                latency_count = 0;
  double latency_sum = 0;
  auto update_state = [&] {
    set_data(snap);
    refresh_window();
  };
  auto stop_recording = [&] {
    if (session && (snap.state == erplay::RecordingState::recording ||
                    snap.state == erplay::RecordingState::paused)) {
      snap.state = erplay::RecordingState::saving;
      update_state();
      try {
        auto result = session->stop();
        snap.state = erplay::RecordingState::ready;
        snap.samples = result.sample_count;
        snap.active_ns = result.duration_ns;
        snap.bytes = 0;
        refresh_library();
        snap.replays = recorder_view().replays;
        if (!snap.replays.empty())
          snap.bytes = snap.replays.front().bytes;
        log_line("Recording finalized samples=" +
                 std::to_string(result.sample_count) +
                 " duration_ns=" + std::to_string(result.duration_ns) +
                 " actual_hz=" + std::to_string(result.actual_rate_hz) +
                 " chunks=" + std::to_string(result.chunk_count)+" fidelity_frames="+std::to_string(result.capture_record_counts[0])+" fidelity_source_drops="+std::to_string(result.capture_source_drops));
      } catch (const std::exception &e) {
        snap.state = erplay::RecordingState::error;
        snap.error = std::wstring(e.what(), e.what() + std::strlen(e.what()));
        log_line(std::string("Finalization failed: ") + e.what());
      }
      session.reset();
      writer.reset();
      update_state();
    }
  };
  auto process_commands = [&] {
    auto pending_commands = take_commands();
    if (pending_commands.empty())
      return;
    while (!pending_commands.empty()) {
      auto c = pending_commands.front();
      pending_commands.pop();
      try {
        if(c==Command::start&&!snap.player){
          snap.error=L"Recording blocked: no live PLAYER_STATE. Wait for Player FOUND in Recorder.";
          log_line("Recording START rejected: no live player sample");
        } else if(c==Command::start&&snap.sample_protocol<3){
          snap.error=L"Fidelity recording requires the new TheaterMode.dll (sample IPC v3). Restart game with the sibling DLL.";
          log_line("Recording START rejected: old sample protocol; new fidelity DLL required");
        } else if (c == Command::start &&
            (snap.state != erplay::RecordingState::recording &&
             snap.state != erplay::RecordingState::paused) &&
            snap.player) {
          erplay::Metadata md;
          md.format_version = 3;
          md.mod_version = "0.7.0-fidelity1";
          md.game_version = "2.7.0.0";
          md.recording_start_unix_ns = unix_ns();
          md.requested_rate_hz = 60.0;
          {
            std::lock_guard names(app.names_mutex);
            if (app.pending_record_name.empty()) app.pending_record_name = library::default_name();
            md.title = app.pending_record_name;
            app.recording_name = app.pending_record_name;
            app.pending_record_name.clear();
          }
          md.tags = "capture-fidelity-v1,raw-state,read-only";
          md.description = "Raw player tracks schema1. New fields REFERENCE; pose, HKS VM, full effect/queue data UNAVAILABLE. Animation reconstruction not implemented.";
          auto final = library::unique_replay_path(app.replays, library::safe_stem(md.title));
          {std::lock_guard names(app.names_mutex);app.recording_path=game_launcher::utf8(fs::absolute(final).wstring());}
          writer = std::make_unique<erplay::Writer>(final, std::move(md), 600);
          session = std::make_unique<erplay::RecordingSession>(*writer);
          session->start();
          snap.capture_frames=0;snap.capture_drops=0;
          character_min_source = monotonic_ns();
          character_known.clear();
          character_last_written.clear();
          character_samples = 0;
          snap.state = erplay::RecordingState::recording;
          snap.error.clear();
          snap.samples = 0;
          snap.bytes = 0;
          snap.active_ns = 0;
          first_source = 0;
          log_line("Recording started: " +
                   game_launcher::utf8(final.wstring()));
        } else if (c == Command::pause && session &&
                   snap.state == erplay::RecordingState::recording) {
          session->pause(monotonic_ns());
          snap.state = erplay::RecordingState::paused;
          log_line("Recording paused");
        } else if (c == Command::resume && session &&
                   snap.state == erplay::RecordingState::paused) {
          character_min_source = monotonic_ns();
          session->resume(character_min_source);
          snap.state = erplay::RecordingState::recording;
          log_line("Recording resumed");
        } else if (c == Command::stop)
          stop_recording();
      } catch (const std::exception &e) {
        snap.state = erplay::RecordingState::error;
        log_line(std::string("Recorder command failed: ") + e.what());
      }
    }
    update_state();
  };
  for (const auto &it : fs::directory_iterator(app.replays)) {
    if (!it.is_regular_file() ||
        !it.path().filename().wstring().ends_with(L".erplay.tmp"))
      continue;
    auto name = it.path();
    name.replace_extension();
    if (fs::exists(name)) {
      auto recovered = name;
      recovered.replace_extension(L".recovered.erplay");
      name = recovered;
    }
    try {
      auto r = erplay::recover_incomplete(it.path(), name);
      log_line("Recovered interrupted recording: " + name.string() +
               " samples=" + std::to_string(r.sample_count));
    } catch (const std::exception &e) {
      log_line(std::string("Incomplete replay recovery failed: ") + e.what());
    }
  }
  refresh_library();
  snap.replays = recorder_view().replays;
  update_state();
  const wchar_t *pipe_name = L"\\\\.\\pipe\\EldenRingTheaterMode_1_17";
  while (!app.stopping.load()) {
    HANDLE pipe =
        CreateNamedPipeW(pipe_name, PIPE_ACCESS_INBOUND,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
                         sizeof(WireMessage), sizeof(WireMessage), 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      app.sample_pipe_ready = false;
      snap.error = L"Could not create named pipe";
      update_state();
      break;
    }
    app.sample_pipe_ready = true;
    refresh_window();
    if (!ConnectNamedPipe(pipe, nullptr) &&
        GetLastError() != ERROR_PIPE_CONNECTED) {
      app.sample_pipe_ready = false;
      CloseHandle(pipe);
      continue;
    }
    DWORD sample_pid = 0;
    GetNamedPipeClientProcessId(pipe, &sample_pid);
    app.game_pid.store(sample_pid);
    snap.connected = true;
    if (last_seq)
      ++snap.reconnects;
    last_seq = 0;
    last_source = 0;
    log_line("IPC connected");
    update_state();
    while (!app.stopping.load()) {
      process_commands();
      DWORD available = 0;
      if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr))
        break;
      if (available < 72) {
        Sleep(5);
        continue;
      }
      WireMessage m{};
      if (!read_message(pipe, m))
        break;
      if (m.magic != MAGIC || (m.version != 1 && m.version != 2 && m.version != 3))
        continue;
      if (m.kind == 4)
        continue;
      if (m.kind == 5) {
        snap.error = L"Game module rejected runtime profile";
        update_state();
        continue;
      }
      if (m.kind == 1)
        continue;
      if (m.kind != 2 && m.kind != 3)
        continue;
      snap.player = m.kind == 2 && m.player_present != 0;
      snap.sample_protocol=m.version;
      snap.action = m.action;
      if (last_seq && m.sequence > last_seq + 1)
        snap.dropped += m.sequence - last_seq - 1;
      if (last_seq && m.sequence <= last_seq)
        log_line("Duplicate/regressing IPC sequence");
      last_seq = m.sequence;
      if (last_source && m.timestamp_ns < last_source) {
        log_line("Player timestamp regression; sample skipped");
        update_state();
        continue;
      }
      last_source = m.timestamp_ns;
      ++window_samples;
      const auto now_ns = monotonic_ns();
      if (now_ns >= m.timestamp_ns) {
        latency_sum += double(now_ns - m.timestamp_ns) / 1e6;
        ++latency_count;
        snap.latency_ms = latency_sum / latency_count;
      }
      process_commands();
      if (snap.player && snap.state == erplay::RecordingState::recording &&
          m.timestamp_ns >= character_min_source) {
        try {
          erplay::Sample s;
          s.source_time_ns = m.timestamp_ns;
          s.position = {m.position[0], m.position[1], m.position[2]};
          s.orientation = {m.quaternion_xyzw[0], m.quaternion_xyzw[1],
                           m.quaternion_xyzw[2], m.quaternion_xyzw[3]};
          if (m.version >= 2) {
            if (!m.action.valid())
              throw std::invalid_argument("invalid native action observation");
            s.action = m.action;
          }
          if(m.version==3){s.capture=m.capture;s.source_sequence=m.sequence;}
          if (!first_source)
            first_source = m.timestamp_ns;
          session->ingest(s);
          if(m.version==3){++snap.capture_frames;snap.capture_drops=m.capture.source_drops;}
          snap.samples = session->accepted_samples();
          snap.active_ns =
              m.timestamp_ns - first_source - session->paused_duration_ns();
          std::error_code ec;
          snap.bytes = fs::file_size(writer->temporary_path(), ec);
          if (ec)
            snap.bytes = 0;
        } catch (const std::exception &e) {
          snap.state = erplay::RecordingState::error;
          log_line(std::string("Sample rejected: ") + e.what());
        }
      }
      auto character_frames = app.characters.take_until(m.timestamp_ns);
      if (session && snap.state == erplay::RecordingState::recording &&
          first_source) {
        try {
          for (const auto &frame : character_frames) {
            if (frame.timestamp < std::max(first_source, character_min_source))
              continue;
            std::map<std::uint64_t, bool> seen;
            for (auto row : frame.rows) {
              seen[row.id] = true;
              if (!character_known.contains(row.id)) {
                auto registry = row;
                registry.kind = erplay::CharacterKind::registry;
                registry.action = {};
                registry.position = {};
                registry.orientation = {0, 0, 0, 1};
                session->ingest_character(registry);
              }
              character_known[row.id] = true;
              const auto previous=character_last_written.find(row.id);
              if(previous!=character_last_written.end()){
                const auto& last=previous->second;double movement=0,dot=0;
                for(unsigned i=0;i<3;++i)movement+=std::pow(double(row.position[i])-last.position[i],2);
                for(unsigned i=0;i<4;++i)dot+=double(row.orientation[i])*last.orientation[i];
                const bool phase_wrap=(row.action.flags&erplay::time_valid)&&row.action.animation_time+0.1f<last.action.animation_time;
                if(movement<0.000001 && std::abs(dot)>0.999999 && row.action.same_action(last.action) && !phase_wrap && row.timestamp_ns-last.timestamp_ns<1'000'000'000ULL)continue;
              }
              character_last_written[row.id]=row;
              row.native_handle = 0;
              row.entity_id = 0;
              row.npc_param = 0;
              row.block_id = 0;
              row.character_type = 0;
              session->ingest_character(row);
              ++character_samples;
            }
            if (!(frame.flags & 2)) {
              for (auto &[id, present] : character_known) {
                if (present && !seen.contains(id)) {
                  erplay::CharacterRecord event;
                  event.id = id;
                  event.timestamp_ns = frame.timestamp;
                  event.kind = erplay::CharacterKind::presence;
                  session->ingest_character(event);
                  character_last_written.erase(id);
                  present = false;
                }
              }
            }
            for(const auto&visual:frame.visuals)session->ingest_visual(visual);
          }
        } catch (const std::exception &e) {
          snap.state = erplay::RecordingState::error;
          snap.error = game_launcher::wide(e.what());
          log_line(std::string("Character capture rejected: ") + e.what());
        }
      }
      const auto now = GetTickCount64();
      if (now - window_start >= 1000) {
        snap.rate =
            double(window_samples) * 1000.0 / double(now - window_start);
        window_samples = 0;
        window_start = now;
        if (snap.state == erplay::RecordingState::recording)
          log_line(
              "CAPTURE_PERF actual_hz=" + std::to_string(snap.rate) +
              " samples=" + std::to_string(snap.samples) +
              " animation_id=" + std::to_string(snap.action.animation_id) +
              " action_flags=" + std::to_string(snap.action.flags) +
              " character_samples=" + std::to_string(character_samples) +
              " character_hz=" + std::to_string(app.characters.stats().rate) +
              " tracked=" + std::to_string(app.characters.stats().tracked) +
              " character_queue=" +
              std::to_string(app.characters.stats().queued) +
              " character_drops=" +
              std::to_string(app.characters.stats().dropped));
        update_state();
      }
    }
    stop_recording();
    app.game_pid.store(0);
    app.sample_pipe_ready = false;
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    snap.connected = false;
    snap.player = false;
    log_line("IPC disconnected");
    update_state();
  }
  stop_recording();
}

void refresh_character_cursor(std::uint64_t time) {
  // Actor reads reuse the indexed Reader stream. Serialize with the playback
  // worker's reads so UI refresh cannot change its seek position mid-decode.
  std::lock_guard playback_lock(app.replay_mutex);
  if (!app.replay_player)
    return;
  static std::string last_error;
  try {
    for (auto &actor : app.character_views)
      actor.current = app.replay_player->reader().character_at(
          actor.info.registry.id, time);
    last_error.clear();
  } catch (const std::exception &e) {
    emergency_stop();
    for (auto &actor : app.character_views)
      actor.current.reset();
    if (last_error != e.what()) {
      last_error = e.what();
      log_line("CHARACTER_CURSOR_ERROR=" + last_error);
      std::lock_guard lock(app.mutex);
      app.data.error = game_launcher::wide(last_error);
    }
  }
}
Snapshot recorder_view() {
  std::lock_guard lock(app.mutex);
  return app.data;
}
void refresh_library() {
  auto files = scan_replays();
  std::lock_guard lock(app.mutex);
  app.data.replays = std::move(files);
}
void open_replay(const fs::path &path) {
  emergency_stop();
  auto next = std::make_unique<replay::Player>(path);
  {
    std::lock_guard lock(app.replay_mutex);
    app.opened_replay = fs::absolute(path);
    app.replay_player = std::move(next);
  }
  app.character_views.clear();
  {
    std::lock_guard lock(app.replay_mutex);
    for (auto info : app.replay_player->reader().characters())
      app.character_views.push_back(
          {info,
           app.replay_player->reader().character_preview(info.registry.id)});
  }
  build_preview_path();
  load_bookmarks();
  log_line("REPLAY_OPEN " + game_launcher::utf8(path.wstring()));
}
void unload_replay() {
  if (!playback_view().loaded) return;
  emergency_stop();
  std::lock_guard lock(app.replay_mutex);
  app.replay_player.reset();
  app.opened_replay.clear();
  app.preview_path.clear();
  app.character_views.clear();
  app.replay_bookmarks.clear();
  app.selected_replay_actor = 0;
  log_line("REPLAY_UNLOAD reader/timeline/actor previews released; files preserved");
}
void bookmark_add() {
  std::lock_guard lock(app.replay_mutex);
  if (!app.replay_player)
    return;
  app.replay_bookmarks.push_back(app.replay_player->state().timestamp_ns);
  std::sort(app.replay_bookmarks.begin(), app.replay_bookmarks.end());
  app.replay_bookmarks.erase(
      std::unique(app.replay_bookmarks.begin(), app.replay_bookmarks.end()),
      app.replay_bookmarks.end());
  save_bookmarks();
}
void bookmark_delete(std::uint64_t t) {
  std::erase(app.replay_bookmarks, t);
  save_bookmarks();
}
void initialize(HWND window) {
  app.window = window;
  app.root = app_root();
  app.replays = app.root / L"replays";
  app.logs = app.root / L"logs";
  fs::create_directories(app.replays);
  fs::create_directories(app.logs);
  app.log.open(app.logs / L"TheaterModeRecorder.log",
               std::ios::binary | std::ios::app);
  load_loader_path();
  {const auto order=library::load_sort(app.root);app.sort_key=static_cast<std::uint32_t>(order.key);app.sort_descending=order.descending;}
  using theater_hotkeys::Action;
  register_hotkey(window, 1, theater_hotkeys::Key(Action::StartRecording), "START RECORDING");
  app.stop_hotkey = register_hotkey(window, 2, theater_hotkeys::Key(Action::StopRecording), "STOP");
  ingame_editor::start();
  app.control.start(app.game_pid);
  app.characters.start(app.game_pid);
  app.worker = std::jthread(pipe_worker);
  app.playback_thread = std::jthread(
      [](std::stop_token stop) { playback_worker::run(stop, playback_tick); });
  log_line(
      "Modern host initialized; dedicated playback worker; native writes OFF");
}
void shutdown() {
  ingame_editor::shutdown();
  app.playback_thread.request_stop();
  if (app.playback_thread.joinable())
    app.playback_thread.join();
  emergency_stop();
  app.launcher.close();
  app.control.close();
  app.stopping = true;
  app.characters.close();
  if (app.worker.joinable()) {
    CancelSynchronousIo(app.worker.native_handle());
    app.worker.join();
  }
  for (int i = 1; i <= 2; ++i)
    UnregisterHotKey(app.window, i);
}
} // namespace theater
