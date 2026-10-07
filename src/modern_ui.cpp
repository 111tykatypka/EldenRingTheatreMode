#include "ingame_editor_server.hpp"
#include "modern_ui.hpp"
#include "editor_math.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <functional>
#include <bit>
#include <string_view>
namespace editor {
using namespace theater;
void zoom_timeline_item(TimeView &view, double duration) {
  ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
  ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
  const auto &io = ImGui::GetIO();
  if (ImGui::IsItemHovered() && io.MouseWheel != 0) {
    if (io.KeyShift) {
      // Own the wheel explicitly: ImGui otherwise maps Shift+wheel to X.
      // Font-relative vertical scrolling stays proportional at high DPI.
      ImGui::SetScrollY(std::clamp(ImGui::GetScrollY() -
          io.MouseWheel * ImGui::GetFontSize() * 5.f, 0.f, ImGui::GetScrollMaxY()));
      return;
    }
    const auto left = ImGui::GetItemRectMin().x;
    const auto width = std::max(1.f, ImGui::GetItemRectSize().x);
    view.zoom(std::pow(.8, io.MouseWheel),
              std::clamp(double(io.MousePos.x - left) / width, 0.0, 1.0), duration);
  }
}
namespace {
std::vector<std::function<void()>> commands;
fs::path selected, last_document;
std::uint64_t chosen_bookmark = UINT64_MAX;
std::uint64_t selected_actor = 0;
std::map<std::uint64_t, bool> hidden, expanded;
TimeView timeline;
bool reset_layout = false, show_diagnostics = true, show_settings = true;
double warning_threshold = 15;
std::string error;

float viewport_zoom = 1;
ImVec2 viewport_pan{};
std::string text(const std::wstring &s) { return game_launcher::utf8(s); }
void label(const std::wstring &s) { ImGui::TextUnformatted(text(s).c_str()); }
void button(const char *caption, std::function<void()> fn) {
  if (ImGui::GetContentRegionAvail().x <
          ImGui::CalcTextSize(caption).x +
              ImGui::GetStyle().FramePadding.x * 2 &&
      ImGui::GetCursorPosX() > ImGui::GetStyle().WindowPadding.x)
    ImGui::NewLine();
  if (ImGui::Button(caption))
    commands.push_back(std::move(fn));
}
void reveal(const fs::path &p) {
  ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
void choose_replay() {
  wchar_t path[32768]{};
  OPENFILENAMEW o{sizeof(o)};
  o.hwndOwner = app.window;
  o.lpstrFilter = L"ERPLAY\0*.erplay\0";
  o.lpstrFile = path;
  o.nMaxFile = 32768;
  o.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (GetOpenFileNameW(&o)) {
    selected = fs::absolute(path);
    open_replay(selected);
  }
}
void request_play(const PlaybackView &, const game_control::State &,
                  bool restart) {

  commands.push_back(restart ? restart_replay : play_replay);
}
void dock_layout() {
  auto id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
  if (reset_layout || !ImGui::DockBuilderGetNode(id)->IsSplitNode()) {
    // Preserve user docking after the first complete layout.
    static bool initialized = false;
    if (initialized && !reset_layout)
      return;
    initialized = true;
    reset_layout = false;
    ImGui::DockBuilderRemoveNode(id);
    ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(id, ImGui::GetMainViewport()->WorkSize);
    auto center = id;
    auto left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, .23f,
                                            nullptr, &center);
    auto right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, .29f,
                                             nullptr, &center);
    auto bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, .40f,
                                              nullptr, &center);
    auto left_bottom =
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, .45f, nullptr, &left);
    ImGui::DockBuilderDockWindow("Replay Library", left);
    ImGui::DockBuilderDockWindow("Recorder", left_bottom);
    ImGui::DockBuilderDockWindow("Launcher", left_bottom);
    ImGui::DockBuilderDockWindow("Viewport", center);
    ImGui::DockBuilderDockWindow("Timeline", bottom);
    for (auto n :
         {"Inspector", "Characters", "Bookmarks", "Diagnostics", "Settings"})
      ImGui::DockBuilderDockWindow(n, right);
    ImGui::DockBuilderFinish(id);
  }
}
void draw_timeline(const PlaybackView &p, const game_control::State &remote) {
  ImGui::Begin("Timeline");
  if (!p.loaded) {
    ImGui::TextDisabled("Open a replay to inspect its tracks.");
    ImGui::End();
    return;
  }
  if (ImGui::Button("Play"))
    request_play(p, remote, false);
  ImGui::SameLine();
  button("|<", [] { seek_replay(0); });
  ImGui::SameLine();
  button("< sample", [] { step_replay(-1); });
  ImGui::SameLine();
  button("sample >", [] { step_replay(1); });
  ImGui::SameLine();
  button("Stop / F6", emergency_stop);
  // Retained alternate UI uses the same continuous contract; no preset selector.
  double timescale=p.state.timescale;
  const double minimum=.001,maximum=10.;
  ImGui::SameLine();ImGui::SetNextItemWidth(160);
  if(ImGui::SliderScalar("Timescale",ImGuiDataType_Double,&timescale,&minimum,&maximum,"%.6fx",ImGuiSliderFlags_Logarithmic)) {
    commands.push_back([timescale] {std::lock_guard lock(app.replay_mutex);if(app.replay_player)app.replay_player->set_timescale(timescale);});
  }
  const double duration = p.summary.duration_ns / 1e9;
  if (ImGui::Button("Fit"))
    timeline.fit(duration);
  ImGui::SameLine();
  button("Beginning", [] { seek_replay(0); });
  ImGui::SameLine();
  button("End", [t = p.summary.duration_ns] { seek_replay(t); });
  ImGui::SameLine();
  button("+ Bookmark", bookmark_add);
  ImGui::SameLine();
  button("Prev marker", [t = p.state.timestamp_ns] {
    auto i = std::lower_bound(app.replay_bookmarks.begin(),
                              app.replay_bookmarks.end(), t);
    if (i != app.replay_bookmarks.begin())
      seek_replay(*--i);
  });
  ImGui::SameLine();
  button("Next marker", [t = p.state.timestamp_ns] {
    auto i = std::upper_bound(app.replay_bookmarks.begin(),
                              app.replay_bookmarks.end(), t);
    if (i != app.replay_bookmarks.end())
      seek_replay(*i);
  });
  label(time_text(p.state.timestamp_ns) + L" / " +
        time_text(p.summary.duration_ns));
  static double jump = 0;
  ImGui::SetNextItemWidth(100);
  ImGui::InputDouble("seconds", &jump, 0, 0, "%.3f");
  ImGui::SameLine();
  button("Seek", [duration] {
    if (!std::isfinite(jump))
      throw std::runtime_error("Seek time must be finite");
    seek_replay(
        static_cast<std::uint64_t>(std::clamp(jump, 0.0, duration) * 1e9));
  });
  ImGui::TextDisabled(
      "Wheel: zoom | Shift+wheel: scroll tracks up/down | Middle drag: pan | Drag ruler: seek (native writes stop)");
  ImGui::BeginChild("Track scroll");
  auto origin = ImGui::GetCursorScreenPos();
  auto size = ImGui::GetContentRegionAvail();
  size.x = std::max(size.x, 1.f);
  float rows_height = 120;
  for (const auto &a : app.character_views)
    if (!hidden[a.info.registry.id])
      rows_height += expanded[a.info.registry.id] ? 72.f : 24.f;
  size.y = std::max(size.y, rows_height);
  ImGui::InvisibleButton("timeline_canvas", size,
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle);
  auto &io = ImGui::GetIO();
  const bool hover = ImGui::IsItemHovered();
  const double local = io.MousePos.x - origin.x;
  zoom_timeline_item(timeline, duration);
  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    timeline.pan(-io.MouseDelta.x / size.x * timeline.span, duration);
  if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
      (io.MousePos.y < origin.y + 100 || local > 220)) {
    auto t = static_cast<std::uint64_t>(timeline.time(local, size.x, duration) *
                                        1e9);
    commands.push_back([t] { seek_replay(t); });
  }
  auto *d = ImGui::GetWindowDrawList();
  d->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
  d->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y},
                   IM_COL32(19, 24, 32, 255));
  const double step =
      std::pow(10.0, std::floor(std::log10(std::max(.001, timeline.span / 8))));
  for (double t = std::ceil(timeline.begin / step) * step;
       t <= timeline.begin + timeline.span; t += step) {
    float x = origin.x + float(timeline.pixel(t, size.x));
    d->AddLine({x, origin.y + 22}, {x, origin.y + size.y},
               IM_COL32(48, 57, 69, 255));
    char b[40];
    snprintf(b, sizeof(b), "%.2fs", t);
    d->AddText({x + 3, origin.y + 3}, IM_COL32(170, 181, 197, 255), b);
  }
  d->AddRectFilled({origin.x, origin.y + 38},
                   {origin.x + size.x, origin.y + 64},
                   IM_COL32(40, 83, 100, 255));
  d->AddText({origin.x + 8, origin.y + 42}, IM_COL32(220, 240, 245, 255),
             "Player / Transform");
  d->AddText({origin.x + 8, origin.y + 76}, IM_COL32(177, 185, 197, 255),
             p.summary.action_event_count
                 ? "Animation / raw observations"
                 : "Animation / unavailable in this recording");
  for (auto t : app.replay_bookmarks) {
    float x = origin.x + float(timeline.pixel(t / 1e9, size.x));
    d->AddTriangleFilled({x, origin.y + 24}, {x - 5, origin.y + 32},
                         {x + 5, origin.y + 32}, IM_COL32(247, 194, 84, 255));
  }
  float x =
      origin.x + float(timeline.pixel(p.state.timestamp_ns / 1e9, size.x));
  d->AddLine({x, origin.y}, {x, origin.y + size.y},
             IM_COL32(100, 219, 212, 255), 2);
  float row_y = origin.y + 108;
  for (const auto &actor : app.character_views) {
    auto id = actor.info.registry.id;
    if (hidden[id])
      continue;
    auto a = origin.x +
             float(timeline.pixel(actor.info.first_ns / 1e9, size.x)),
         b = origin.x + float(timeline.pixel(actor.info.last_ns / 1e9, size.x));
    d->AddRectFilled({a, row_y}, {b, row_y + 18},
                     selected_actor == id ? IM_COL32(111, 92, 147, 255)
                                          : IM_COL32(67, 58, 88, 255));
    auto name = std::string(expanded[id] ? "v Actor " : "> Actor ") +
                std::to_string(id) + " / Transform";
    d->AddText({origin.x + 8, row_y}, IM_COL32(220, 220, 230, 255),
               name.c_str());
    if (hover && ImGui::IsMouseClicked(0) && io.MousePos.y >= row_y &&
        io.MousePos.y < row_y + 24) {
      selected_actor = id; app.selected_replay_actor=id;
      if (local < 220)
        expanded[id] = !expanded[id];
    }
    row_y += 24;
    if (expanded[id]) {
      d->AddText({origin.x + 24, row_y}, IM_COL32(170, 180, 195, 255),
                 "Raw animation / Unknown semantics");
      for (const auto &r : actor.trajectory)
        if (r.kind == erplay::CharacterKind::transform && r.action.flags & 1) {
          float tick_x =
              origin.x + float(timeline.pixel(r.timestamp_ns / 1e9, size.x));
          d->AddLine({tick_x, row_y + 14}, {tick_x, row_y + 20},
                     IM_COL32(170, 142, 203, 255));
        }
      row_y += 24;
      d->AddText({origin.x + 24, row_y}, IM_COL32(170, 180, 195, 255),
                 "Observed presence (not spawn/death)");
      row_y += 24;
    }
  }
  d->PopClipRect();
  if (hover)
    ImGui::SetTooltip("%.3f s", timeline.time(local, size.x, duration));
  ImGui::EndChild();
  ImGui::End();
}
void viewport(const PlaybackView &p) {
  ImGui::Begin("Viewport");
  if (ImGui::Button("Fit trajectory")) {
    viewport_zoom = 1;
    viewport_pan = {};
  }
  ImGui::SameLine();
  ImGui::TextDisabled("X/Z data view | wheel zoom, middle drag");
  auto o = ImGui::GetCursorScreenPos();
  auto sz = ImGui::GetContentRegionAvail();
  sz.x = std::max(1.f, sz.x);
  sz.y = std::max(1.f, sz.y);
  ImGui::InvisibleButton("trajectory", sz, ImGuiButtonFlags_MouseButtonMiddle);
  if (ImGui::IsItemHovered())
    viewport_zoom = std::clamp(
        viewport_zoom * std::pow(1.1f, ImGui::GetIO().MouseWheel), .05f, 100.f);
  if (ImGui::IsItemActive() &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    viewport_pan.x += ImGui::GetIO().MouseDelta.x;
    viewport_pan.y += ImGui::GetIO().MouseDelta.y;
  }
  auto *d = ImGui::GetWindowDrawList();
  d->PushClipRect(o, {o.x + sz.x, o.y + sz.y}, true);
  d->AddRectFilled(o, {o.x + sz.x, o.y + sz.y}, IM_COL32(17, 21, 28, 255));
  if (p.loaded && !app.preview_path.empty()) {
    float minx = app.preview_path[0].x, maxx = minx,
          minz = app.preview_path[0].z, maxz = minz;
    for (auto v : app.preview_path) {
      minx = std::min(minx, v.x);
      maxx = std::max(maxx, v.x);
      minz = std::min(minz, v.z);
      maxz = std::max(maxz, v.z);
    }
    for (const auto &actor : app.character_views) {
      if (hidden[actor.info.registry.id])
        continue;
      for (const auto &r : actor.trajectory) {
        if (r.kind != erplay::CharacterKind::transform)
          continue;
        minx = std::min(minx, r.position[0]);
        maxx = std::max(maxx, r.position[0]);
        minz = std::min(minz, r.position[2]);
        maxz = std::max(maxz, r.position[2]);
      }
    }
    float scale = std::max(1.f, std::min(sz.x - 40, sz.y - 40)) /
                  std::max({maxx - minx, maxz - minz, 1.f}) * viewport_zoom;
    auto project = [&](erplay::Vec3 v) {
      return ImVec2{
          o.x + sz.x / 2 + (v.x - (minx + maxx) / 2) * scale + viewport_pan.x,
          o.y + sz.y / 2 - (v.z - (minz + maxz) / 2) * scale + viewport_pan.y};
    };
    for (const auto &actor : app.character_views) {
      auto id = actor.info.registry.id;
      if (hidden[id])
        continue;
      ImVec4 color;
      ImGui::ColorConvertHSVtoRGB(float(std::fmod(id * .61803398875, 1.0)),
                                  .55f, .85f, color.x, color.y, color.z);
      color.w = 1;
      const auto packed = ImGui::ColorConvertFloat4ToU32(color);
      std::optional<ImVec2> previous;
      const erplay::CharacterRecord *current =
          actor.current ? &*actor.current : nullptr;
      for (const auto &r : actor.trajectory) {
        if (r.kind != erplay::CharacterKind::transform) {
          previous.reset();
          continue;
        }
        auto point = project({r.position[0], r.position[1], r.position[2]});
        if (previous)
          d->AddLine(*previous, point, packed,
                     selected_actor == id ? 2.f : 1.f);
        previous = point;
      }
      if (current && current->kind == erplay::CharacterKind::transform &&
          p.state.timestamp_ns - current->timestamp_ns < 1'000'000'000ULL) {
        auto point = project(
            {current->position[0], current->position[1], current->position[2]});
        d->AddCircleFilled(point, 4, packed);
        if (selected_actor == id)
          d->AddCircle(point, 8, IM_COL32_WHITE, 0, 2);
        auto name = "Actor " + std::to_string(id);
        d->AddText({point.x + 8, point.y}, packed, name.c_str());
      }
    }
    for (size_t i = 1; i < app.preview_path.size(); ++i)
      d->AddLine(project(app.preview_path[i - 1]), project(app.preview_path[i]),
                 IM_COL32(69, 125, 142, 255), 1.5f);
    auto start = project(app.preview_path.front()),
         end = project(app.preview_path.back()),
         current = project(p.state.position);
    d->AddCircleFilled(start, 4, IM_COL32(128, 207, 130, 255));
    d->AddText(start, IM_COL32(200, 220, 200, 255), "START");
    d->AddCircle(end, 5, IM_COL32(244, 187, 96, 255));
    d->AddCircleFilled(current, 5, IM_COL32(106, 224, 215, 255));
    d->AddCircle(current, 8, IM_COL32_WHITE);
    d->AddText({current.x + 12, current.y}, IM_COL32_WHITE, "Player");
  }
  d->PopClipRect();
  ImGui::End();
}
} // namespace
void load_settings() {
  std::ifstream f(app.root / L"Modern.settings");
  double value = 15;
  if (f >> value && std::isfinite(value) && value >= 0)
    warning_threshold = value;
}
void save_settings() {
  std::ofstream f(app.root / L"Modern.settings");
  f << warning_threshold;
}
void dispatch() {
  theater::ingame_editor::poll();
  auto pending = std::move(commands);
  commands.clear();
  for (auto &fn : pending)
    try {
      fn();
    } catch (const std::exception &e) {
      error = e.what();
      log_line("EDITOR_ERROR=" + error);
    }
}
void draw(const Snapshot &recorder, const PlaybackView &p,
          const game_control::State &remote,
          const game_launcher::State &launch) {
  selected_actor=app.selected_replay_actor;
  if (last_document != app.opened_replay) {
    last_document = app.opened_replay;
    timeline.fit(p.summary.duration_ns / 1e9);
    viewport_zoom = 1;
    viewport_pan = {};
    chosen_bookmark = UINT64_MAX;
    selected_actor = 0;app.selected_replay_actor=0;
    hidden.clear();
    expanded.clear();
  }
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open replay..."))
        commands.push_back(choose_replay);
      if (ImGui::MenuItem("Unload"))
        commands.push_back(unload_replay);
      if (ImGui::MenuItem("Replay folder"))
        commands.push_back([] { reveal(app.replays); });
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
      ImGui::MenuItem("Diagnostics", nullptr, &show_diagnostics);
      ImGui::MenuItem("Settings", nullptr, &show_settings);
      if (ImGui::MenuItem("Reset docking"))
        reset_layout = true;
      ImGui::EndMenu();
    }
    ImGui::Text("  GAME %s | IPC %s | PLAYER %s | 1.17 / 2.7.0.0",
                app.game_pid.load() ? "RUNNING" : "OFF",
                remote.connected ? "CONNECTED" : "OFF",
                recorder.player ? "FOUND" : "WAITING");
    ImGui::EndMainMenuBar();
  }
  dock_layout();
  ImGui::Begin("Replay Library");
  button("Open...", choose_replay);
  ImGui::SameLine();
  button("Refresh", refresh_library);
  ImGui::SameLine();
  button("Folder", [] { reveal(app.replays); });
  if (ImGui::BeginTable(
          "replays", 9,
          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
              ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX |
              ImGuiTableFlags_ScrollY,
          {0, std::max(100.f, ImGui::GetContentRegionAvail().y - 76)})) {
    for (auto n : {"Replay", "Date UTC", "Game", "Duration", "Samples",
                   "Characters", "Actions", "Format", "Size MB"})
      ImGui::TableSetupColumn(n);
    ImGui::TableHeadersRow();
    for (auto &e : recorder.replays) {
      auto path = e.path;
      auto id = text(path.wstring());
      ImGui::PushID(id.c_str());
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (ImGui::Selectable(text(path.filename().wstring()).c_str(),
                            selected == path,
                            ImGuiSelectableFlags_SpanAllColumns |
                                ImGuiSelectableFlags_AllowDoubleClick)) {
        selected = path;
        if (ImGui::IsMouseDoubleClicked(0))
          commands.push_back([path] { open_replay(path); });
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s\nGame %s", id.c_str(),
                          e.summary.metadata.game_version.c_str());
      ImGui::TableNextColumn();
      const auto recorded = static_cast<std::time_t>(
          e.summary.metadata.recording_start_unix_ns / 1'000'000'000ULL);
      std::tm date{};
      char date_text[32]{};
      if (gmtime_s(&date, &recorded) == 0)
        std::strftime(date_text, sizeof(date_text), "%Y-%m-%d %H:%M:%S", &date);
      ImGui::TextUnformatted(date_text);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(e.summary.metadata.game_version.c_str());
      ImGui::TableNextColumn();
      label(time_text(e.summary.duration_ns));
      ImGui::TableNextColumn();
      ImGui::Text("%llu", e.summary.sample_count);
      ImGui::TableNextColumn();
      ImGui::Text("%llu", e.summary.character_count);
      ImGui::TableNextColumn();
      ImGui::Text("%llu", e.summary.action_event_count);
      ImGui::TableNextColumn();
      ImGui::Text("v%u", e.summary.metadata.format_version);
      ImGui::TableNextColumn();
      ImGui::Text("%.2f", double(e.bytes) / 1048576);
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  ImGui::BeginDisabled(selected.empty());
  button("Open selected", [] { open_replay(selected); });
  ImGui::SameLine();
  button("Duplicate", [] {
    auto out =
        selected.parent_path() / (selected.stem().wstring() + L"_copy.erplay");
    for (unsigned i = 2; fs::exists(out); ++i)
      out = selected.parent_path() / (selected.stem().wstring() + L"_copy_" +
                                      std::to_wstring(i) + L".erplay");
    fs::copy_file(selected, out);
    refresh_library();
  });
  ImGui::SameLine();
  if (ImGui::Button("Rename"))
    ImGui::OpenPopup("Rename replay");
  ImGui::SameLine();
  if (ImGui::Button("Delete"))
    ImGui::OpenPopup("Delete replay");
  ImGui::EndDisabled();
  button("Unload", unload_replay);
  if (ImGui::BeginPopupModal("Rename replay", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    static char name[512]{};
    ImGui::InputText("New filename", name, sizeof(name));
    if (ImGui::Button("Rename")) {
      auto source = selected;
      auto target = source.parent_path() / fs::path(game_launcher::wide(name));
      if (target.extension() != L".erplay")
        target += L".erplay";
      commands.push_back([source, target] {
        if (target.parent_path() != source.parent_path() || fs::exists(target))
          throw std::runtime_error("Choose an unused filename in this folder");
        const bool loaded = app.opened_replay == source;
        if (loaded)
          unload_replay();
        fs::rename(source, target);
        auto a = source, b = target;
        a += L".bookmarks";
        b += L".bookmarks";
        if (fs::exists(a))
          fs::rename(a, b);
        selected = target;
        refresh_library();
        if (loaded)
          open_replay(target);
      });
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
  if (ImGui::BeginPopupModal("Delete replay", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    label(selected.filename().wstring());
    ImGui::TextWrapped("Permanently delete this recording and its bookmarks?");
    if (ImGui::Button("Delete permanently")) {
      auto path = selected;
      commands.push_back([path] {
        if (app.opened_replay == path)
          unload_replay();
        fs::remove(path);
        auto b = path;
        b += L".bookmarks";
        fs::remove(b);
        selected.clear();
        refresh_library();
      });
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
  ImGui::End();
  ImGui::Begin("Recorder");
  const char *states[]{"IDLE",   "RECORDING", "PAUSED",
                       "SAVING", "READY",     "ERROR"};
  ImGui::Text("%s", states[int(recorder.state)]);
  label(time_text(recorder.active_ns));
  ImGui::Text("Samples %llu | %.2f Hz\nData %.2f MB | Drops %llu",
              recorder.samples, recorder.rate, double(recorder.bytes) / 1048576,
              recorder.dropped);
  ImGui::Text("Sample IPC v%u | Fidelity frames %llu | source queue drops %llu",unsigned(recorder.sample_protocol),recorder.capture_frames,recorder.capture_drops);
  ImGui::TextDisabled("%u raw fields / 9 tracks; new reads require runtime correlation",unsigned(std::size(erplay::capture_fields)));
  auto cs = app.characters.stats();
  ImGui::Text("Characters %llu | %.1f Hz | queue %llu/16 | drops %llu/%llu",
              cs.tracked, cs.rate, cs.queued, cs.dropped, cs.source_drops);
  ImGui::Text("Rejected character messages: %llu", cs.rejected);
  ImGui::BeginDisabled(p.active || !recorder.player);
  button("Start / F5", [] { post_command(Command::start); });
  ImGui::EndDisabled();
  ImGui::SameLine();
  button("Stop / F6", emergency_stop);
  label(recorder.error);
  ImGui::End();
  ImGui::Begin("Launcher");
  label(launch.diagnostic);
  ImGui::BeginDisabled(launch.busy() || app.game_pid.load() != 0 ||
                       !app.stop_hotkey || !app.sample_pipe_ready);
  button("Start Elden Ring", launch_game);
  ImGui::EndDisabled();
  button("Choose YAFSML...", choose_loader);
  label(app.loader_path.wstring());
  ImGui::BeginDisabled(launch.busy() || app.game_pid.load()!=0);
  button("Choose Elden Ring EXE...", choose_game);
  ImGui::EndDisabled();
  label(app.game_path.wstring());
  button("Launch logs", [] { reveal(app.root / L"launch"); });
  ImGui::End();
  ImGui::Begin("Inspector");
  ImGui::TextUnformatted(
      p.loaded ? text(app.opened_replay.filename().wstring()).c_str()
               : "No replay loaded");
  ImGui::TextWrapped("%s", text(p.diagnostic).c_str());
  ImGui::BeginDisabled(!p.loaded);
  if (ImGui::Button(app.game_pid.load() ? "Play in game"
                                        : "Play"))
    request_play(p, remote, false);
  ImGui::SameLine();
  if (ImGui::Button("Restart"))
    request_play(p, remote, true);
  ImGui::EndDisabled();
  ImGui::BeginDisabled(!p.loaded||p.active||!remote.ready||recorder.state==erplay::RecordingState::recording||recorder.state==erplay::RecordingState::saving);
  button("Return to replay start",return_replay_start);
  ImGui::EndDisabled();
  ImGui::TextWrapped("Play prepares the recorded start automatically when a loaded scene anchor matches. No cross-map teleport.");
  if(p.loaded){std::lock_guard lock(app.replay_mutex);const auto start=app.replay_player->reader().sample(0);
    const double dx=start.position.x-remote.live.position[0],dy=start.position.y-remote.live.position[1],dz=start.position.z-remote.live.position[2];
    ImGui::Text("Recorded start: %.3f %.3f %.3f | distance %.2f",start.position.x,start.position.y,start.position.z,std::sqrt(dx*dx+dy*dy+dz*dz));}
  ImGui::Separator();
  ImGui::Text("Position: %.4f  %.4f  %.4f", p.state.position.x,
              p.state.position.y, p.state.position.z);
  ImGui::Text("Quaternion: %.4f  %.4f  %.4f  %.4f", p.state.orientation.x,
              p.state.orientation.y, p.state.orientation.z,
              p.state.orientation.w);
  ImGui::Text("Raw animation: %d | semantics UNKNOWN",
              p.state.current_action.animation_id);
  ImGui::Text("Recorded native phase: %.4fs / %.4fs | source: %s",
              p.state.current_action.animation_time,p.state.current_action.animation_length,
              p.state.dense_action?"continuous capture":"legacy sparse events");
  ImGui::Text("Game: %s | Samples: %llu",
              p.summary.metadata.game_version.c_str(), p.summary.sample_count);
  if(p.loaded){std::lock_guard lock(app.replay_mutex);auto visual=app.replay_player->reader().visual_at(selected_actor,p.state.timestamp_ns);
    if(visual){ImGui::Separator();ImGui::Text("Recorded model %u | ground bits %u",visual->model,visual->ground);
      if(visual->flags&2)ImGui::Text("Recorded HP %u / %u",visual->hp,visual->max_hp);else ImGui::TextDisabled("HP UNAVAILABLE");
      if(visual->flags&8){ImGui::Text("Equipment slots L%u R%u | arm style %u",visual->left_slot,visual->right_slot,visual->arm_style);for(unsigned i=0;i<22;++i){ImGui::Text("Slot %u: native param %d",i,visual->equipment[i]);}}
      if(visual->flags&16)ImGui::Text("Native bounded face snapshot: 288 bytes | body archetype %u",visual->archetype);
      ImGui::TextDisabled("Captured state only; equipment/face/HP are not written into the game.");
    }
  }
  if(p.loaded && ImGui::CollapsingHeader("Player capture tracks (read only)")){
    std::lock_guard lock(app.replay_mutex);
    auto &reader=app.replay_player->reader();
    ImGui::TextDisabled("Raw REFERENCE observations; no animation/pose reconstruction promised.");
    ImGui::Text("Source queue drops: %llu",p.summary.capture_source_drops);
    for(const auto&track:erplay::capture_tracks){
      ImGui::PushID(int(track.id));
      if(ImGui::TreeNode(track.name,"%s (%llu samples)",track.name,p.summary.capture_record_counts[track.id-5])){
        auto record=reader.capture_at(track.id,p.state.timestamp_ns);
        if(!record)ImGui::TextDisabled("UNAVAILABLE in this replay");
        else for(const auto&field:erplay::capture_fields){if(field.track!=track.id)continue;
          const auto local=field.offset-track.begin;bool available=true;
          for(unsigned i=0;i<field.words;++i)available&=record->available(local+i);
          if(!available){ImGui::TextDisabled("%s: UNAVAILABLE",field.name);continue;}
          if(field.words==1 && std::string_view(field.kind)=="f32"){
            const auto value=std::bit_cast<float>(record->values[local]);
            ImGui::Text("%s: %.6g [raw %08X]%s",field.name,double(value),record->values[local],std::isfinite(value)?"":" NONFINITE");
          }else if(field.words==1){const auto bits=record->values[local];const auto value=std::string_view(field.kind)=="i16"?int(static_cast<std::int16_t>(bits)):std::bit_cast<std::int32_t>(bits);ImGui::Text("%s: %d [raw %08X]",field.name,value,bits);}
          else {ImGui::Text("%s: %u words",field.name,field.words);for(unsigned i=0;i<field.words;++i){if(i%4)ImGui::SameLine();ImGui::Text("%08X",record->values[local+i]);}}
        }
        ImGui::TreePop();
      }
      ImGui::PopID();
    }
    ImGui::TextDisabled("Pose / HKS VM / blend weights / proxy velocity / full SpEffect list: UNAVAILABLE.");
  }
  for (const auto &actor : app.character_views)
    if (actor.info.registry.id == selected_actor) {
      ImGui::Separator();
      ImGui::Text("Selected actor %llu | raw type %u", selected_actor,
                  actor.info.registry.character_type);
      ImGui::Text("Native handle %llX | block %d",
                  actor.info.registry.native_handle,
                  actor.info.registry.block_id);
      ImGui::Text("First %.3fs | Last %.3fs | Samples %llu",
                  actor.info.first_ns / 1e9, actor.info.last_ns / 1e9,
                  actor.info.samples);
      if (actor.current)
        ImGui::Text("Current raw animation %d | source %.3fs",
                    actor.current->action.animation_id,
                    actor.current->timestamp_ns / 1e9);
      ImGui::TextDisabled(
          "Presence is observational, not a spawn/death assertion.\nViewport "
          "trajectories are decimated for display.");
    }
  ImGui::End();
  draw_timeline(p, remote);
  viewport(p);
  ImGui::Begin("Bookmarks");
  button("Add current time", bookmark_add);
  for (auto t : app.replay_bookmarks) {
    ImGui::PushID(std::to_string(t).c_str());
    if (ImGui::Selectable(text(time_text(t)).c_str(), chosen_bookmark == t,
                          ImGuiSelectableFlags_AllowDoubleClick)) {
      chosen_bookmark = t;
      if (ImGui::IsMouseDoubleClicked(0))
        commands.push_back([t] { seek_replay(t); });
    }
    ImGui::PopID();
  }
  ImGui::BeginDisabled(chosen_bookmark == UINT64_MAX);
  button("Jump", [] { seek_replay(chosen_bookmark); });
  ImGui::SameLine();
  button("Remove", [] {
    bookmark_delete(chosen_bookmark);
    chosen_bookmark = UINT64_MAX;
  });
  ImGui::EndDisabled();
  ImGui::End();
  ImGui::Begin("Characters");
  ImGui::Text("%zu recorded observational lifetimes",
              app.character_views.size());
  if (ImGui::BeginTable("actors", 5,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_ScrollY)) {
    for (auto n : {"Actor", "Entity", "NPC param", "Samples", "Visible"})
      ImGui::TableSetupColumn(n);
    ImGui::TableHeadersRow();
    for (const auto &v : app.character_views) {
      auto id = v.info.registry.id;
      ImGui::PushID(std::to_string(id).c_str());
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (ImGui::Selectable(("Actor " + std::to_string(id)).c_str(),
                            selected_actor == id)) {
        selected_actor = id; app.selected_replay_actor=id;
      }
      ImGui::TableNextColumn();
      ImGui::Text("%u", v.info.registry.entity_id);
      ImGui::TableNextColumn();
      ImGui::Text("%d", v.info.registry.npc_param);
      ImGui::TableNextColumn();
      ImGui::Text("%llu", v.info.samples);
      ImGui::TableNextColumn();
      bool visible = !hidden[id];
      if (ImGui::Checkbox("##visible", &visible))
        hidden[id] = !visible;
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  ImGui::End();
  if (show_settings) {
    ImGui::Begin("Settings", &show_settings);
    ImGui::Checkbox("Developer XZ-only diagnostic (native Y; not final replay)", &app.xz_diagnostic);
    ImGui::TextWrapped("Play can prepare start using a matching stationary scene anchor. Return is bounded to the loaded area; map loading is not implemented.");
    ImGui::InputDouble("Legacy warning preference (unused)", &warning_threshold, 1, 10,
                       "%.1f");
    warning_threshold =
        std::isfinite(warning_threshold) ? std::max(0., warning_threshold) : 15;
    button("Save settings", save_settings);
    ImGui::BeginDisabled(p.active);
    ImGui::Checkbox("Experimental native animation requests + same-ID cycles", &app.animation);
    ImGui::TextWrapped("Uses recorded native phase to detect cycles; phase/pose are NOT forced. Verify first 5 seconds before longer playback.");
    ImGui::Checkbox("Experimental existing-character transform replay", &app.actor_playback);
    ImGui::Checkbox("Replay selected NPC only (player writes OFF)", &app.selected_actor_only);
    app.selected_replay_actor=selected_actor;
    ImGui::Text("Selected replay actor: %llu",static_cast<unsigned long long>(selected_actor));
    if(app.selected_actor_only&&selected_actor==0)ImGui::TextWrapped("Select a non-player track in Characters before starting. No track selected: playback will be rejected.");
    static int limit = 0;
    if (ImGui::Combo("Native duration", &limit,
                     "Full replay\0First 2 seconds\0First 5 seconds\0First 10 seconds\0"))
      app.limit_ns = limit == 1   ? 2'000'000'000ULL
                     : limit == 2 ? 5'000'000'000ULL
                     : limit == 3 ? 10'000'000'000ULL
                                  : 0;
    ImGui::EndDisabled();
    static float radius = 200, hz = 60;
    static int radius_preset=2;
    if(ImGui::Combo("Capture radius preset",&radius_preset,"Near (50)\0Medium (100)\0Wide (200)\0Custom\0")){
      if(radius_preset<3)radius=radius_preset==0?50.f:radius_preset==1?100.f:200.f;
    }
    static int budget = 1024;
    ImGui::InputFloat("Character radius", &radius);
    ImGui::InputFloat("Capture Hz", &hz);
    ImGui::InputInt("Capture buffer slots", &budget);
    button("Save capture settings (next DLL load)", [] {
      if (!std::isfinite(radius) || radius <= 0 || !std::isfinite(hz) ||
          hz < 1 || hz > 120 || budget < 1 || budget > 16384)
        throw std::runtime_error("Capture settings: positive radius, 1..120 "
                                 "Hz, 1..16384 buffer slots");
      std::ofstream f(app.root / L"Modern.capture.ini");
      f << "radius=" << radius << "\nhz=" << hz << "\nbudget=" << budget
        << "\n";
    });
    ImGui::TextWrapped(
        "Input lock: old flags FAILED AT RUNTIME; new producer-stage "
        "neutralization EXPERIMENTAL. Grounding: diagnostics only, unresolved. "
        "Actor transform replay: opt-in, matching existing actors only; AI "
        "ownership EXPERIMENTAL. No spawning, resurrection, equipment/VFX "
        "restoration or verified gait reproduction. Exit radius = enter x 1.2.");
    ImGui::End();
  }
  if (show_diagnostics) {
    ImGui::Begin("Diagnostics", &show_diagnostics);
    ImGui::Text("UI %.1f FPS | worker %.2f Hz | IPC %.2f Hz",
                ImGui::GetIO().Framerate, p.clock_hz, remote.replay_send_hz);
    ImGui::Text("Sample IPC latency %.3f ms | reconnects %llu",
                recorder.latency_ms, recorder.reconnects);
    label(remote.diagnostic);
    ImGui::TextWrapped("%s", error.c_str());
    button("Recorder logs", [] { reveal(app.logs); });
    ImGui::SameLine();
    button("Game log",
           [] { reveal(fs::temp_directory_path() / L"TheaterModeGame.log"); });
    button("Collect tester logs", [] {
      for(const auto*name:{L"TheaterModeGame.log",L"TheaterModeLocomotionTrace.log",L"TheaterModeRuntimeTrace.jsonl", L"TheaterModeRender.log"}){
        auto source=fs::temp_directory_path()/name;
        if(fs::exists(source))fs::copy_file(source,app.logs/name,fs::copy_options::overwrite_existing);
      }
      reveal(app.logs);
    });
    button("Runtime differential trace (10s)", [] { log_line("RUNTIME_TRACE_HOST_START queued="+std::to_string(app.control.runtime_trace(true))); });
    ImGui::SameLine();button("Stop runtime trace", [] { app.control.runtime_trace(false); });
    ImGui::TextWrapped("Read-only trace: LIVE or replay; PreBehavior, before/after PostPhysics writes. Stops after 10 seconds. Unknown proxy/ground fields are null.");
    ImGui::BeginDisabled(p.active||selected_actor==0);
    static int ownership_mode=4;
    ImGui::Combo("Selected NPC experiment",&ownership_mode,"noMove only\0noAttack only\0noMove + noAttack\0noUpdate only\0animationSpeed = 0\0");
    if(ownership_mode==3)ImGui::TextWrapped("noUpdate BLOCKED: native update lifecycle not yet verified.");
    ImGui::TextWrapped("noMove/noAttack: exact +0x538 static consumers verified; DLL requires live callback/owner layout check. Two-second reversible experiment only.");
    ImGui::BeginDisabled(ownership_mode==3);
    button("Run selected NPC experiment (2s)", [] {
      std::lock_guard lock(app.replay_mutex);
      if(app.replay_player){for(const auto& a:app.replay_player->reader().characters())if(a.registry.id==selected_actor){
        log_line("OWNERSHIP_PROBE_HOST selected_id="+std::to_string(selected_actor)+" mode="+std::to_string(ownership_mode+1)+" queued="+std::to_string(app.control.probe_actor(a.registry,ownership_mode+1)));break;
      }}
    });
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::TextWrapped("Developer experiment: one exact existing NPC, 2s, captured values restored on Stop/disconnect/timeout. These are not verified AI ownership. Debug-camera control mode: NOT IMPLEMENTED.");
    button("Trace start", [] { app.control.trace(game_control::trace_start); });
    ImGui::SameLine();
    button("Trace stop", [] { app.control.trace(game_control::trace_stop); });
    button("Probe +0.5 X once", [] {
      if (!playback_view().active)
        app.control.nudge();
    });
    ImGui::End();
  }
  if (!ImGui::GetIO().WantCaptureKeyboard) {
    if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
      if (p.state.status != replay::Status::playing)
        request_play(p, remote, false);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
      commands.push_back([] { step_replay(-1); });
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
      commands.push_back([] { step_replay(1); });
  }
}
} // namespace editor
