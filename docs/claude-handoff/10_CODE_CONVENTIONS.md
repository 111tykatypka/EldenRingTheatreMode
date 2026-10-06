# Conventions and working rules

Snapshot2026-10-06, source3d97070. Follow adjacent code; avoid unrelated rewrites.

## Style and errors

C++20, UNICODE/_UNICODE/NOMINMAX, MSVC/utf-8/EHsc. Rust2024 cdylib TheaterMode. Rust module/functions snake_case; native/protocol structs generally PascalCase; constants constexpr/uppercase. No universal formatting configuration was established. Existing compact files are not reason to reformat the architecture.

C++ validators throw specific errors; host reports meaningful status. Rust/native failures stop mutation and log explicit state. Access violation diagnostics are not permission to swallow faults or continue unsafe writes. Win32/graphics ownership must restore/release correctly. Native engine lifetime follows its task/factory/removal APIs; no best-effort guessed destruction.

## Logs/settings/hotkeys

Game `%TEMP%\TheaterModeGame.log`, render/crash logs inTEMP, host `%LOCALAPPDATA%\EldenRingTheaterMode\logs\TheaterModeRecorder.log`. Rust asynchronous log queue; meaningful transitions by default, verbose optional. NATIVE_GHOST messages feed HUD; NATIVE_REPLAY_DATA excluded. Log actual values/guard reason, not optimistic success.

Settings/layout live underLOCALAPPDATAEldenRingTheaterMode; Modern.layout.ini layout. Concrete config anchors appended below; don't invent a global JSON schema or compression feature.

F5start/F6stop-emergency/F7pause/F8resume historical recorder mapping. Ghost F10create/F11remove scoped to game foreground. Payload feature conflicts with those keys. Insert cyclesClean0/Transport1/Editor2, cursor/capture onlyEditor. Wheel zooms both directions; Shiftwheel scrolls tracks. Simplified production UI omits Pause/Resume buttons, internal compatibility states remain.

## Adding hooks

1. Establish exact target bytes/RVA/ABI/ownership/thread/callers; correlate SDK/RTTI.
2. Read-only instrumentation first, strict profile+entry guards. No guessed writes.
3. Create owned hooks before enabling; rollback only them on failure, preserve original calls/returns.
4. IPC/render workers queue copied commands; mutations only verified game/native context, reacquire/validate current objects.
5. DefaultOFF, stop on invalid state/playerloss/disconnect; native cleanup only. Never retain/re-resolve retired pointers/handles.
6. Isolated Release manifest/test procedure; real user acceptance before verified label. No hot unload.

## Adding UI panels

Standalone modern_ui consumes editor_backend snapshots/commands. In-game renderer uses bounded EditorSnapshot/Request, no direct player pointers or duplicate replay clock. Add versioned protocol data/tests when needed. ImGui context only render/UI thread. Preserve hotkeys, globalStop, originalWndProc forwarding and input queue reentrancy safety.

## Owner working rules

Read current code/history/notes first; do not create project again or replace verified integration without strong reason. Preserve golden master and original game/read-only references. Existing YAFSML, no custom injector/EAC bypass. Offline existing authorized workflow only. No proprietary code/assets/binaries copied. No arbitrary product limits/fake fields/performance. Incremental smallest component, live validation gates; build/unit tests are not in-game success. When user action needed, provide exact output/close/launch/save/key/observation/log instructions.

## Concrete source anchors (implementation HEAD 3d97070)

Lines refer to original code, not the appended prose. Long lines are truncated for display; inspect source before edits.

### `src/editor_backend.cpp`

```text
src/editor_backend.cpp:7: struct WireMessage {
src/editor_backend.cpp:83: log_line(std::string("REPLAY_WORKER_ERROR=") + e.what());
src/editor_backend.cpp:92: "REPLAY_PERF host_clock_hz=" + std::to_string(app.clock_hz) +
src/editor_backend.cpp:230: L"Launcher settings", MB_ICONERROR | MB_OK);
src/editor_backend.cpp:247: }catch(const std::exception&e){MessageBoxW(app.window,game_launcher::wide(e.what()).c_str(),L"Game settings",MB_ICONERROR|MB_OK);}
src/editor_backend.cpp:270: log_line("REPLAY_START refused: recorder command pending");
src/editor_backend.cpp:277: log_line("REPLAY_START refused: F6 unavailable or recorder active");
src/editor_backend.cpp:354: if (RegisterHotKey(w, id, MOD_NOREPEAT, key)) {
src/editor_backend.cpp:370: app.root / L"launch" / L"YAFSML.ini"};
src/editor_backend.cpp:452: log_line("Recording finalized samples=" +
src/editor_backend.cpp:563: CreateNamedPipeW(pipe_name, PIPE_ACCESS_INBOUND,
src/editor_backend.cpp:810: log_line("REPLAY_OPEN " + game_launcher::utf8(path.wstring()));
```

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
