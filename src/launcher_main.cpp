// One-click Theater Mode launcher. Replaces the old docked editor window.
//
// The window has one job: start Elden Ring through the established YAFSML workflow with
// TheaterMode.dll injected. The host backend (recorder, replay storage, playback clock,
// overlay pipe, F5/F6 hotkeys) keeps running inside this process, because the in-game
// overlay drives it. Everything else is done in game (F4).
#include "editor_backend.hpp"
#include "ingame_editor_server.hpp"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "TheaterTheme.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <wrl/client.h>
#include <fstream>
#include <sstream>
using Microsoft::WRL::ComPtr;
using namespace TheaterUI::Theme;

namespace {
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGISwapChain> swapchain;
ComPtr<ID3D11RenderTargetView> target;
UINT width{}, height{};
bool done{};
float dpi = 1;
bool russian = false;

bool create_target() {
  ComPtr<ID3D11Texture2D> texture;
  return SUCCEEDED(swapchain->GetBuffer(0, IID_PPV_ARGS(&texture))) &&
         SUCCEEDED(device->CreateRenderTargetView(texture.Get(), nullptr, &target));
}
bool create_device(HWND w) {
  DXGI_SWAP_CHAIN_DESC d{};
  d.BufferCount = 2;
  d.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  d.OutputWindow = w;
  d.SampleDesc.Count = 1;
  d.Windowed = TRUE;
  d.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  D3D_FEATURE_LEVEL level{};
  const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
  auto hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                          D3D11_SDK_VERSION, &d, &swapchain, &device, &level, &context);
  if (hr == DXGI_ERROR_UNSUPPORTED)
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                       D3D11_SDK_VERSION, &d, &swapchain, &device, &level, &context);
  return SUCCEEDED(hr) && create_target();
}

// Language is shared with the in-game overlay (TheaterOverlay.ini, key "language").
fs::path overlay_settings() {
  wchar_t base[MAX_PATH]{};
  const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
  if (!n || n >= MAX_PATH) return {};
  return fs::path(base) / L"EldenRingTheaterMode" / L"TheaterOverlay.ini";
}
// Launcher settings shared with the overlay file: language, debug_console. Other keys are kept.
void load_settings() {
  std::ifstream in(overlay_settings());
  std::string key;
  float value = 0;
  while (in >> key >> value) {
    if (key == "language") russian = value >= 1;
    else if (key == "debug_console") theater::app.show_debug_console = value >= 1;
  }
}
void save_settings() {
  const auto path = overlay_settings();
  if (path.empty()) return;
  std::stringstream kept;
  {
    std::ifstream in(path);
    std::string key, value;
    while (in >> key >> value)
      if (key != "language" && key != "debug_console") kept << key << ' ' << value << '\n';
  }
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::trunc);
  out << "language " << (russian ? 1 : 0) << '\n'
      << "debug_console " << (theater::app.show_debug_console.load() ? 1 : 0) << '\n'
      << kept.str();
}

const char *tr(const char *en, const char *ru) { return russian ? ru : en; }
std::string text(const std::wstring &w) { return game_launcher::utf8(w); }
void open_path(const fs::path &p) { ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

// Launcher type scale, in logical px at 100% Windows scaling (multiplied by the monitor DPI).
// About 30% larger than the overlay's dense editor sizes, so it reads at a glance.
namespace Type {
constexpr float Title = 26, Subtitle = 15, Section = 14, Body = 16, Small = 15, Button = 20;
}
// Readable colours on PanelBgSolid: primary for values, secondary (not muted) for labels.
constexpr Rgba LabelText{178, 184, 193, 255};

Fonts fonts;
void push(Font role, float px) { ImGui::PushFont(fonts[role], px * dpi); }
float S(float v) { return Px(v, dpi); }

void section_title(const char *title) {
  push(Font::PanelTitle, Type::Section);
  ImGui::PushStyleColor(ImGuiCol_Text, LabelText.Vec4());
  ImGui::TextUnformatted(title);
  ImGui::PopStyleColor();
  ImGui::PopFont();
  ImGui::Dummy(ImVec2(0, S(4)));
}

// A status line: coloured dot, label, value.
void status_row(const char *label, const char *value, Rgba dot) {
  push(Font::Body, Type::Body);
  auto *dl = ImGui::GetWindowDrawList();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float h = ImGui::GetTextLineHeight();
  dl->AddCircleFilled(ImVec2(p.x + S(5), p.y + h * 0.5f), S(5), dot.U32());
  ImGui::SetCursorScreenPos(ImVec2(p.x + S(20), p.y));
  ImGui::PushStyleColor(ImGuiCol_Text, LabelText.Vec4());
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  ImGui::SameLine(S(200));
  ImGui::TextUnformatted(value);
  ImGui::PopFont();
  ImGui::Dummy(ImVec2(0, S(2)));
}

void draw_launcher() {
  using namespace theater;
  const auto launch = app.launcher.state();
  const auto control = app.control.state();
  const bool game_running = app.game_pid.load() != 0;
  const bool ready = app.stop_hotkey && app.sample_pipe_ready.load();
  const float pad = S(32);

  const ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(vp->Size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, S(26)));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(12), S(7)));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(S(10), S(10)));
  ImGui::Begin("##launcher", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  auto *dl = ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
  const float right = o.x + size.x - pad;

  // Header: brand diamond, title, game version; language switch on the right.
  const float r = S(11);
  const ImVec2 c(o.x + pad + r, o.y + S(26) + S(20));
  dl->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y), Color::AccentGold.U32());
  ImGui::SetCursorScreenPos(ImVec2(c.x + r + S(14), o.y + S(22)));
  ImGui::BeginGroup();
  push(Font::BodyStrong, Type::Title);
  ImGui::TextUnformatted("THEATER MODE");
  ImGui::PopFont();
  push(Font::Body, Type::Subtitle);
  ImGui::PushStyleColor(ImGuiCol_Text, LabelText.Vec4());
  ImGui::TextUnformatted("Elden Ring 1.17");
  ImGui::PopStyleColor();
  ImGui::PopFont();
  ImGui::EndGroup();
  const float header_bottom = ImGui::GetItemRectMax().y;
  {
    push(Font::BodyStrong, Type::Small);
    const float w = S(52), h = S(34);
    ImGui::SetCursorScreenPos(ImVec2(right - w * 2 - S(6), o.y + S(30)));
    for (int i = 0; i < 2; ++i) {
      const bool active = russian == (i == 1);
      ImGui::PushStyleColor(ImGuiCol_Button, (active ? Color::SelectedBg : Color::FrameBg).Vec4());
      ImGui::PushStyleColor(ImGuiCol_Text, (active ? Color::TextPrimary : LabelText).Vec4());
      if (ImGui::Button(i ? "RU" : "EN", ImVec2(w, h)) && !active) {
        russian = i == 1;
        save_settings();
      }
      ImGui::PopStyleColor(2);
      if (!i) ImGui::SameLine(0, S(6));
    }
    ImGui::PopFont();
  }
  const float divider = header_bottom + S(18);
  dl->AddLine(ImVec2(o.x, divider), ImVec2(o.x + size.x, divider), Color::Border.U32());

  // Status.
  ImGui::SetCursorScreenPos(ImVec2(o.x + pad, divider + S(20)));
  section_title(tr("STATUS", "СОСТОЯНИЕ"));
  const bool starting = launch.busy() && !game_running;
  status_row("Elden Ring",
             game_running ? tr("Running", "Запущена") : starting ? tr("Starting", "Запуск") : tr("Not running", "Не запущена"),
             game_running ? Color::AccentGreen : starting ? Color::AccentAmber : Color::TextMuted);
  status_row("Theater Mode",
             control.connected ? tr("Injected", "Внедрён") : game_running ? tr("Waiting", "Ожидание") : "-",
             control.connected ? Color::AccentGreen : game_running ? Color::AccentAmber : Color::TextMuted);
  status_row(tr("Player", "Игрок"), control.ready ? tr("Found", "Найден") : "-",
             control.ready ? Color::AccentGreen : Color::TextMuted);

  // The one button.
  ImGui::Dummy(ImVec2(0, S(8)));
  const bool can_launch = !launch.busy() && !game_running && ready;
  const char *label = game_running ? tr("Elden Ring is running", "Elden Ring запущена")
                      : launch.busy() ? tr("Starting...", "Запуск...")
                                      : tr("Launch Elden Ring", "Запустить Elden Ring");
  push(Font::BodyStrong, Type::Button);
  ImGui::PushStyleColor(ImGuiCol_Button, (can_launch ? Color::AccentBlue : Color::FrameBg).Vec4());
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Rgba{104, 164, 228, 255}.Vec4());
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, Rgba{70, 128, 190, 255}.Vec4());
  ImGui::PushStyleColor(ImGuiCol_Text, (can_launch ? Color::TextOnAccent : LabelText).Vec4());
  ImGui::BeginDisabled(!can_launch);
  if (ImGui::Button(label, ImVec2(-1, S(60)))) launch_game();
  ImGui::EndDisabled();
  ImGui::PopStyleColor(4);
  ImGui::PopFont();

  // What the launcher is doing, or why it failed.
  push(Font::Body, Type::Small);
  const bool error = launch.phase == game_launcher::Phase::error;
  ImGui::PushStyleColor(ImGuiCol_Text, (error ? Rgba{236, 118, 110, 255} : LabelText).Vec4());
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(ready ? text(launch.diagnostic).c_str()
                               : tr("Preparing the recorder and the F5/F6 hotkeys...", "Подготовка записи и клавиш F5/F6..."));
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
  ImGui::PopFont();

  // Settings at the bottom: paths, folders, debug console, hotkeys.
  push(Font::Body, Type::Small);
  const float row = ImGui::GetFrameHeight();
  const float footer_h = S(20) + row * 4 + S(10) * 3 + ImGui::GetTextLineHeight() + S(26);
  const float footer = std::max(ImGui::GetCursorScreenPos().y + S(12), o.y + size.y - footer_h);
  dl->AddLine(ImVec2(o.x, footer), ImVec2(o.x + size.x, footer), Color::Border.U32());
  ImGui::SetCursorScreenPos(ImVec2(o.x + pad, footer + S(20)));
  const bool idle = !launch.busy() && !game_running;
  const float button_w = ImGui::CalcTextSize(tr("Change...", "Изменить...")).x + S(28);
  auto path_row = [&](const char *name, const fs::path &value, void (*change)(), bool enabled) {
    ImGui::PushID(name);
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, LabelText.Vec4());
    ImGui::TextUnformatted(name);
    ImGui::PopStyleColor();
    ImGui::SameLine(S(150));
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float avail = right - button_w - S(12) - at.x;
    const std::string v = value.empty() ? std::string(tr("Not set", "Не задано")) : text(value.wstring());
    // Long paths keep their end (the file name) visible.
    std::string shown = v;
    while (shown.size() > 4 && ImGui::CalcTextSize(shown.c_str()).x > avail) shown = "..." + shown.substr(std::min<size_t>(shown.size(), 4));
    ImGui::PushStyleColor(ImGuiCol_Text, (value.empty() ? Color::AccentAmber : Color::TextPrimary).Vec4());
    ImGui::TextUnformatted(shown.c_str());
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", v.c_str());
    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(right - button_w, at.y));
    ImGui::BeginDisabled(!enabled);
    if (ImGui::Button(tr("Change...", "Изменить..."), ImVec2(button_w, 0))) change();
    ImGui::EndDisabled();
    ImGui::PopID();
  };
  path_row(tr("Game", "Игра"), app.game_path, choose_game, idle);
  path_row(tr("Mod loader", "Загрузчик"), app.loader_path, choose_loader, idle);
  if (ImGui::Button(tr("Replays folder", "Папка записей"))) open_path(app.replays);
  ImGui::SameLine();
  if (ImGui::Button(tr("Logs", "Журналы"))) open_path(app.logs);
  ImGui::SameLine(0, S(24));
  bool console = app.show_debug_console.load();
  ImGui::BeginDisabled(!idle);
  if (ImGui::Checkbox(tr("Show debug console", "Показывать консоль отладки"), &console)) {
    app.show_debug_console = console;
    save_settings();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip("%s", tr("Off: no console window. YAFSML still writes its own log file.",
                               "Выкл.: окна консоли нет. YAFSML всё равно пишет свой файл журнала."));
  ImGui::Dummy(ImVec2(0, S(2)));
  ImGui::PushStyleColor(ImGuiCol_Text, LabelText.Vec4());
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(tr("In game:  F4 Theater Mode    F5 record    F6 stop    Space play/pause",
                            "В игре:  F4 Theater Mode    F5 запись    F6 стоп    Пробел пуск/пауза"));
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
  ImGui::PopFont();

  ImGui::End();
  ImGui::PopStyleVar(3);
}
} // namespace

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK launcher_proc(HWND w, UINT m, WPARAM a, LPARAM b) {
  if (m == WM_APP + 77) { theater::ingame_editor::poll(); return 0; }
  if (m == WM_HOTKEY) {
    // RegisterHotKey is application-wide; F5/F6 work while the game has focus.
    theater::handle_global_hotkey(static_cast<UINT>(a));
    return 0;
  }
  if (ImGui_ImplWin32_WndProcHandler(w, m, a, b)) return 1;
  switch (m) {
  case WM_SIZE:
    if (a != SIZE_MINIMIZED) { width = LOWORD(b); height = HIWORD(b); }
    return 0;
  case WM_DPICHANGED: {
    dpi = float(HIWORD(a)) / 96;
    auto *r = reinterpret_cast<RECT *>(b);
    SetWindowPos(w, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
  }
  case WM_ERASEBKGND: return 1;
  case WM_CLOSE: done = true; return 0;
  case WM_DESTROY: PostQuitMessage(0); return 0;
  case WM_SYSCOMMAND:
    if ((a & 0xfff0) == SC_KEYMENU) return 0;
  }
  return DefWindowProcW(w, m, a, b);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  const HANDLE single = CreateMutexW(nullptr, FALSE, L"Local\\EldenRingTheaterMode_1_17_Host");
  if (!single) return 1;
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    if (auto w = FindWindowW(L"EldenRingTheaterModeWindow", nullptr)) {
      ShowWindow(w, SW_RESTORE);
      SetForegroundWindow(w);
    }
    CloseHandle(single);
    return 0;
  }
  // Per-monitor DPI awareness v2: Windows never bitmap-stretches the window, so text stays sharp,
  // and WM_DPICHANGED arrives when the window moves to a monitor with different scaling.
  if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) ImGui_ImplWin32_EnableDpiAwareness();
  POINT cursor{};
  GetCursorPos(&cursor);
  dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY));
  WNDCLASSW wc{};
  wc.lpfnWndProc = launcher_proc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.lpszClassName = L"EldenRingTheaterModeWindow";
  RegisterClassW(&wc);
  const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  RECT rect{0, 0, int(680 * dpi), int(640 * dpi)};
  AdjustWindowRectExForDpi(&rect, style, FALSE, 0, UINT(dpi * 96 + 0.5f));
  HWND window = CreateWindowW(wc.lpszClassName, L"Elden Ring Theater Mode", style, CW_USEDEFAULT, CW_USEDEFAULT,
                              rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);
  if (!window || !create_device(window)) {
    MessageBoxW(nullptr, L"DirectX 11 renderer initialization failed.", L"Theater Mode", MB_ICONERROR);
    CloseHandle(single);
    return 1;
  }
  const BOOL dark = TRUE; // DWMWA_USE_IMMERSIVE_DARK_MODE, matches the overlay's dark surfaces
  DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  fonts = LoadFonts(io);
  ImGui_ImplWin32_Init(window);
  ImGui_ImplDX11_Init(device.Get(), context.Get());
  theater::initialize(window);
  theater::refresh_library();
  load_settings();
  ShowWindow(window, SW_SHOWNORMAL);
  float applied_dpi = 0;
  bool failed = false;
  while (!done) {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
      if (msg.message == WM_QUIT) done = true;
    }
    if (done) break;
    if (IsIconic(window)) {
      // Keep serving the overlay pipe and playback cursor while minimized.
      theater::refresh_character_cursor(theater::playback_view().state.timestamp_ns);
      MsgWaitForMultipleObjects(0, nullptr, FALSE, 40, QS_ALLINPUT);
      continue;
    }
    if (width && height) {
      target.Reset();
      auto hr = swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
      width = height = 0;
      if (FAILED(hr) || !create_target()) { failed = true; break; }
    }
    if (applied_dpi != dpi) {
      ApplyStyle(ImGui::GetStyle(), dpi);
      auto &st = ImGui::GetStyle();
      st.FontScaleDpi = 1.0f;
      st.Colors[ImGuiCol_WindowBg] = Color::PanelBgSolid.Vec4();
      st.Colors[ImGuiCol_Button] = Color::FrameBg.Vec4();
      applied_dpi = dpi;
    }
    theater::refresh_character_cursor(theater::playback_view().state.timestamp_ns);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    push(Font::Body, Type::Body);
    draw_launcher();
    ImGui::PopFont();
    ImGui::Render();
    const auto bg = Color::PanelBgSolid.Vec4();
    const float clear[]{bg.x, bg.y, bg.z, 1};
    auto *view = target.Get();
    context->OMSetRenderTargets(1, &view, nullptr);
    context->ClearRenderTargetView(view, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    auto hr = swapchain->Present(1, 0);
    theater::ingame_editor::poll();
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) { failed = true; break; }
    if (hr == DXGI_STATUS_OCCLUDED) MsgWaitForMultipleObjects(0, nullptr, FALSE, 40, QS_ALLINPUT);
  }
  theater::shutdown();
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  target.Reset();
  swapchain.Reset();
  context.Reset();
  device.Reset();
  DestroyWindow(window);
  CloseHandle(single);
  if (failed)
    MessageBoxW(nullptr, L"Graphics device lost. Replay writes stopped. Restart Theater Mode.", L"Theater Mode",
                MB_ICONERROR);
  return failed ? 1 : 0;
}
