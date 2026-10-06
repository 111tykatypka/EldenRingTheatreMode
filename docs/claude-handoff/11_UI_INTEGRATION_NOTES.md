# UI integration notes

Snapshot2026-10-06, implementation3d97070. Claude UI artifacts not provided; recommended slots below do not assert their actual API.

## Standalone UI

modern_main.cpp WinMain ownsWin32/DX11device/swapchain/message/renderloop. ImGui_ImplWin32_EnableDpiAwareness; resizable1440×900 initial logical size scaled byDPI, shown maximized. ImGui1.92.5 docking/navkeyboard, StyleColorsDark. Roundings window3/frame4/grab4; dark background(.075,.09,.12,1), header(.16,.29,.34,1). Windows segoeui.ttf17 or default fallback. LayoutModern.layout.ini. Explicit Russian glyph coverage unverified.

modern_ui panels: ReplayLibrary,Recorder,Launcher,Viewport(compact X/Z),Timeline,Inspector,Diagnostics,Settings. Backend models/snapshots in editor_backend; ReplayPlayer/Clock not UI-owned. Abstract trajectory is not game world rendering.

## Injected UI

TheaterRenderBackend.cpp owns separate DX12 ImGui context on render thread. Adopts actual direct queue/swapchain; per-backbuffer allocator/RTV/fence, descriptorheap128. DPI via GetDpiForWindow/FontScaleDpi/style scaling. Current default font, Cyrillic coverage unverified. Present processes queued input, draws, transitions/submits then original. Resize rebuilds resources.

WndProc original forwarding must be published before installation. No mutex held across reentrant ImGuiWin32 handling. Explicit mouse events avoid render-thread SetCapture/ReleaseCapture clobber. Rendering exceptions disable overlay/forward original; diagnostic fault logging does not make unsafe execution valid.

Insert0Clean/1Transport/2Editor. FirstInsert no interactive cursor is normal transport behavior; nextEditor cursor/capture. Editor pipe polls about50ms. Native status HUD top-right independent ofInsert/Clean: yellow pending60s countdown, red error, blue transitions, lastmessage persists. Export tm_render_native_status copies string into synchronized buffer. Noninteractive; must not claim successful engine operation merely because button clicked.

## Integration contracts

New theme apply after each ImGuiCreateContext; layout solver in host/overlay draw routines; copied view-model from backend or protocol snapshot; command queue into existing backend/EditorRequest. Engine pointers never escape to UI. No second parser/clock. Version snapshot before adding camera/ghost data. Actual Claude theme/header/layout API unavailable.

|Needed data|Available now|Gap|
|---|---|---|
|Replay time/duration/speed/state|ReplayPlayer/backend; EditorSnapshot time_ns/duration_ns/playback_speed/phase|No newclock|
|Recording/stats/files|Recorder/backend snapshots|Overlay snapshot lacks all recorder stats; versioned extension|
|Connection/player/liveposition|Telemetry/backend; connected/player_found/live_position|Orientation/details need model/protocol review|
|Recorded actor selection/list|Charactertracks/backend;16actors/page|Live list completeness not proven|
|Nativeghost status/list|Local status string, one owned epoch record|No generalized ghost registry/overlay list contract|
|Camera keys/tracks/FOV/path|Not verified implemented|Need actual CameraController/CameraTrack backend|
|Bookmarks|Host sidecar/timeline|Not engine/world state|
|Diagnostic/errors|Backend,256charEditor diagnostic, native HUD|Show actual ack/error|

Preserve F5/F6 regardless ImGui capture, wheel zoom both directions, Shiftwheel verticalscroll. UI must not block game thread or change ownership. New visuals can proceed independently of native lifecycle but cannot imply a working camera/full encounter.

## Concrete source anchors (implementation HEAD 3d97070)

Lines refer to original code, not the appended prose. Long lines are truncated for display; inspect source before edits.

### `src/modern_main.cpp`

```text
src/modern_main.cpp:50: // RegisterHotKey is application-wide, independent of foreground ImGui focus.
src/modern_main.cpp:84: int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
src/modern_main.cpp:117: ImGui::CreateContext();
src/modern_main.cpp:121: ImGui::StyleColorsDark();
src/modern_main.cpp:132: io.Fonts->AddFontFromFileTTF(game_launcher::utf8(font.wstring()).c_str(),
src/modern_main.cpp:135: io.Fonts->AddFontDefault();
src/modern_main.cpp:139: editor::load_settings();
src/modern_main.cpp:141: game_launcher::utf8((theater::app.root / L"Modern.layout.ini").wstring());
src/modern_main.cpp:173: ImGui::GetStyle().FontScaleDpi = dpi;
src/modern_main.cpp:202: editor::save_settings();
```

### `src/modern_ui.cpp`

```text
src/modern_ui.cpp:36: bool reset_layout = false, show_diagnostics = true, show_settings = true;
src/modern_ui.cpp:107: ImGui::Begin("Timeline");
src/modern_ui.cpp:176: ImGui::BeginChild("Track scroll");
src/modern_ui.cpp:278: ImGui::Begin("Viewport");
src/modern_ui.cpp:380: void load_settings() {
src/modern_ui.cpp:381: std::ifstream f(app.root / L"Modern.settings");
src/modern_ui.cpp:386: void save_settings() {
src/modern_ui.cpp:387: std::ofstream f(app.root / L"Modern.settings");
src/modern_ui.cpp:394: for (auto &fn : pending)
src/modern_ui.cpp:416: if (ImGui::BeginMainMenuBar()) {
src/modern_ui.cpp:417: if (ImGui::BeginMenu("File")) {
src/modern_ui.cpp:426: if (ImGui::BeginMenu("View")) {
src/modern_ui.cpp:428: ImGui::MenuItem("Settings", nullptr, &show_settings);
src/modern_ui.cpp:440: ImGui::Begin("Replay Library");
src/modern_ui.cpp:446: if (ImGui::BeginTable(
src/modern_ui.cpp:499: ImGui::BeginDisabled(selected.empty());
src/modern_ui.cpp:519: if (ImGui::BeginPopupModal("Rename replay", nullptr,
src/modern_ui.cpp:552: if (ImGui::BeginPopupModal("Delete replay", nullptr,
src/modern_ui.cpp:576: ImGui::Begin("Recorder");
src/modern_ui.cpp:590: ImGui::BeginDisabled(p.active || !recorder.player);
src/modern_ui.cpp:597: ImGui::Begin("Launcher");
src/modern_ui.cpp:599: ImGui::BeginDisabled(launch.busy() || app.game_pid.load() != 0 ||
src/modern_ui.cpp:605: ImGui::BeginDisabled(launch.busy() || app.game_pid.load()!=0);
src/modern_ui.cpp:611: ImGui::Begin("Inspector");
src/modern_ui.cpp:616: ImGui::BeginDisabled(!p.loaded);
src/modern_ui.cpp:624: ImGui::BeginDisabled(!p.loaded||p.active||!remote.ready||recorder.state==erplay::RecordingState::recording||recorder.state==erplay::RecordingState::saving);
src/modern_ui.cpp:700: ImGui::Begin("Bookmarks");
src/modern_ui.cpp:712: ImGui::BeginDisabled(chosen_bookmark == UINT64_MAX);
src/modern_ui.cpp:721: ImGui::Begin("Characters");
src/modern_ui.cpp:724: if (ImGui::BeginTable("actors", 5,
src/modern_ui.cpp:754: if (show_settings) {
src/modern_ui.cpp:755: ImGui::Begin("Settings", &show_settings);
src/modern_ui.cpp:762: button("Save settings", save_settings);
src/modern_ui.cpp:763: ImGui::BeginDisabled(p.active);
src/modern_ui.cpp:788: button("Save capture settings (next DLL load)", [] {
src/modern_ui.cpp:791: throw std::runtime_error("Capture settings: positive radius, 1..120 "
src/modern_ui.cpp:793: std::ofstream f(app.root / L"Modern.capture.ini");
src/modern_ui.cpp:806: ImGui::Begin("Diagnostics", &show_diagnostics);
src/modern_ui.cpp:827: ImGui::BeginDisabled(p.active||selected_actor==0);
src/modern_ui.cpp:832: ImGui::BeginDisabled(ownership_mode==3);
```

### `native_ui/TheaterRenderBackend.cpp`

```text
native_ui/TheaterRenderBackend.cpp:29: struct Frame{ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12Resource> buffer;D3D12_CPU_DESCRIPTOR_HANDLE rtv{};UINT64 fence{};};
native_ui/TheaterRenderBackend.cpp:30: struct Input{HWND hwnd;UINT msg;WPARAM w;LPARAM l;};
native_ui/TheaterRenderBackend.cpp:31: class TheaterRenderBackend {
native_ui/TheaterRenderBackend.cpp:81: context=ImGui::CreateContext();ImGui::SetCurrentContext(context);auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;ImGui::StyleColorsDark();auto dpi=GetDpiForWindow(hwnd)/96.f;ImGui::GetStyle().FontScaleDpi=dpi;ImGui::GetStyle().ScaleAll
native_ui/TheaterRenderBackend.cpp:94: ImGui::Begin("Native Ghost Status",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoDocking);
native_ui/TheaterRenderBackend.cpp:104: ImGui::Begin(editor?"Theater Editor — experimental":"Theater Transport — experimental");
native_ui/TheaterRenderBackend.cpp:106: ImGui::BeginDisabled(!s.loaded);if(ImGui::Button("Play"))command(theater_ui::play);ImGui::SameLine();if(ImGui::Button("Stop / F6"))command(theater_ui::stop);
native_ui/TheaterRenderBackend.cpp:183: extern "C" void tm_render_native_status(const char* text){auto&b=backend();{std::lock_guard lock(b.native_status_mutex);strncpy_s(b.native_status,text,_TRUNCATE);b.native_status_tick=GetTickCount64();}b.native_status_visible=true;}
native_ui/TheaterRenderBackend.cpp:190: struct Hook{void*target,*detour;void**original;};Hook hooks[]{{s[8],reinterpret_cast<void*>(on_present),reinterpret_cast<void**>(&b.present)},{s[13],reinterpret_cast<void*>(on_resize),reinterpret_cast<void**>(&b.resize)},{f[10],reinterpret_cast<void*>(on_create),reinterpret_cast<
native_ui/TheaterRenderBackend.cpp:191: for(auto&hook:hooks){if(!ok)break;ok=MH_CreateHook(hook.target,hook.detour,hook.original)==MH_OK;if(ok)b.targets.push_back(hook.target);}
native_ui/TheaterRenderBackend.cpp:204: auto&b=backend();auto*c=ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;io.Fonts->AddFontDefault();
```

### `shared/TheaterUiProtocol.h`

```text
shared/TheaterUiProtocol.h:7: enum Command : std::uint32_t { poll, play, pause, stop, restart, seek, previous, next, speed, select, page };
shared/TheaterUiProtocol.h:8: struct Request { std::uint32_t magic_value{magic},version{1},command{};std::uint32_t reserved{};std::uint64_t sequence{},value{}; };
shared/TheaterUiProtocol.h:9: struct Actor {std::uint64_t id{},handle{};std::uint32_t entity{},type{};std::int32_t npc{};std::uint32_t reserved{};};
shared/TheaterUiProtocol.h:11: struct Snapshot {std::uint32_t magic_value{magic},version{1},loaded{},active{},phase{},count{},offset{},total{};
```
