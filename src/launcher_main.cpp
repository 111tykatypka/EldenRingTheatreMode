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
void load_language() {
  std::ifstream in(overlay_settings());
  std::string key;
  float value = 0;
  while (in >> key >> value)
    if (key == "language") russian = value >= 1;
}
void save_language() {
  const auto path = overlay_settings();
  if (path.empty()) return;
  std::stringstream kept;
  {
    std::ifstream in(path);
    std::string key, value;
    while (in >> key >> value)
      if (key != "language") kept << key << ' ' << value << '\n';
  }
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::trunc);
  out << "language " << (russian ? 1 : 0) << '\n' << kept.str();
}

const char *tr(const char *en, const char *ru) { return russian ? ru : en; }
std::string text(const std::wstring &w) { return game_launcher::utf8(w); }
void open_path(const fs::path &p) { ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

Fonts fonts;
void push(Font role, float extra = 1.0f) { ImGui::PushFont(fonts[role], fonts.size[(int)role] * dpi * extra); }

// A status line: coloured dot, label, value.
void status_row(const char *label, const char *value, Rgba dot) {
  auto *dl = ImGui::GetWindowDrawList();
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const float h = ImGui::GetTextLineHeight();
  dl->AddCircleFilled(ImVec2(p.x + Px(4, dpi), p.y + h * 0.5f), Px(3.5f, dpi), dot.U32());
  ImGui::SetCursorScreenPos(ImVec2(p.x + Px(16, dpi), p.y));
  ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  ImGui::SameLine(Px(150, dpi));
  ImGui::TextUnformatted(value);
}

void draw_launcher() {
  using namespace theater;
  const auto launch = app.launcher.state();
  const auto control = app.control.state();
  const bool game_running = app.game_pid.load() != 0;
  const bool ready = app.stop_hotkey && app.sample_pipe_ready.load();
  const float s = dpi;

  const ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(vp->Size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(Px(28, s), Px(24, s)));
  ImGui::Begin("##launcher", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  auto *dl = ImGui::GetWindowDrawList();
  const ImVec2 o = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();

  // Header: brand diamond, title, game version; language switch on the right.
  const ImVec2 c(o.x + Px(36, s), o.y + Px(40, s));
  const float r = Px(9, s);
  dl->AddQuadFilled(ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y),
                    Color::AccentGold.U32());
  ImGui::SetCursorScreenPos(ImVec2(o.x + Px(56, s), o.y + Px(24, s)));
  push(Font::BodyStrong, 1.35f);
  ImGui::TextUnformatted("THEATER MODE");
  ImGui::PopFont();
  ImGui::SetCursorScreenPos(ImVec2(o.x + Px(56, s), ImGui::GetCursorScreenPos().y - Px(2, s)));
  push(Font::Meta);
  ImGui::PushStyleColor(ImGuiCol_Text, Color::TextMuted.Vec4());
  ImGui::TextUnformatted("Elden Ring 1.17");
  ImGui::PopStyleColor();
  ImGui::PopFont();
  {
    const float w = Px(44, s);
    ImGui::SetCursorScreenPos(ImVec2(o.x + size.x - Px(28, s) - w * 2 - Px(4, s), o.y + Px(28, s)));
    ImGui::PushStyleColor(ImGuiCol_Button, Rgba{0, 0, 0, 0}.Vec4());
    for (int i = 0; i < 2; ++i) {
      const bool active = russian == (i == 1);
      ImGui::PushStyleColor(ImGuiCol_Text, (active ? Color::TextPrimary : Color::TextMuted).Vec4());
      if (active) ImGui::PushStyleColor(ImGuiCol_Button, Color::SelectedBg.Vec4());
      if (ImGui::Button(i ? "RU" : "EN", ImVec2(w, Px(26, s))) && !active) {
        russian = i == 1;
        save_language();
      }
      if (active) ImGui::PopStyleColor();
      ImGui::PopStyleColor();
      if (!i) ImGui::SameLine(0, Px(4, s));
    }
    ImGui::PopStyleColor();
  }
  const float divider = o.y + Px(76, s);
  dl->AddLine(ImVec2(o.x, divider), ImVec2(o.x + size.x, divider), Color::BorderSubtle.U32());

  // Status card.
  ImGui::SetCursorScreenPos(ImVec2(o.x + Px(28, s), divider + Px(18, s)));
  push(Font::PanelTitle);
  ImGui::PushStyleColor(ImGuiCol_Text, Color::TextMuted.Vec4());
  ImGui::TextUnformatted(tr("STATUS", "СОСТОЯНИЕ"));
  ImGui::PopStyleColor();
  ImGui::PopFont();
  ImGui::Dummy(ImVec2(0, Px(2, s)));
  const bool starting = launch.busy() && !game_running;
  status_row(tr("Elden Ring", "Elden Ring"),
             game_running ? tr("Running", "Запущена") : starting ? tr("Starting", "Запуск") : tr("Not running", "Не запущена"),
             game_running ? Color::AccentGreen : starting ? Color::AccentAmber : Color::TextDisabled);
  status_row(tr("Theater Mode", "Theater Mode"),
             control.connected ? tr("Injected", "Внедрён") : game_running ? tr("Waiting", "Ожидание") : "-",
             control.connected ? Color::AccentGreen : game_running ? Color::AccentAmber : Color::TextDisabled);
  status_row(tr("Player", "Игрок"), control.ready ? tr("Found", "Найден") : "-",
             control.ready ? Color::AccentGreen : Color::TextDisabled);

  // The one button.
  ImGui::Dummy(ImVec2(0, Px(14, s)));
  const bool can_launch = !launch.busy() && !game_running && ready;
  const char *label = game_running ? tr("Elden Ring is running", "Elden Ring запущена")
                      : launch.busy() ? tr("Starting...", "Запуск...")
                                      : tr("Launch Elden Ring", "Запустить Elden Ring");
  push(Font::BodyStrong, 1.2f);
  ImGui::PushStyleColor(ImGuiCol_Button, (can_launch ? Color::AccentBlue : Color::FrameBg).Vec4());
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Rgba{104, 164, 228, 255}.Vec4());
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, Rgba{70, 128, 190, 255}.Vec4());
  ImGui::PushStyleColor(ImGuiCol_Text, (can_launch ? Color::TextOnAccent : Color::TextSecondary).Vec4());
  ImGui::BeginDisabled(!can_launch);
  if (ImGui::Button(label, ImVec2(-1, Px(52, s)))) launch_game();
  ImGui::EndDisabled();
  ImGui::PopStyleColor(4);
  ImGui::PopFont();

  // Launcher diagnostic: what it is doing, or why it failed.
  push(Font::Meta);
  const bool error = launch.phase == game_launcher::Phase::error;
  ImGui::PushStyleColor(ImGuiCol_Text, (error ? Color::AccentRed : Color::TextMuted).Vec4());
  ImGui::PushTextWrapPos(0.0f);
  if (!ready)
    ImGui::TextUnformatted(tr("Preparing the recorder and the F5/F6 hotkeys...", "Подготовка записи и клавиш F5/F6..."));
  else
    ImGui::TextUnformatted(text(launch.diagnostic).c_str());
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
  ImGui::PopFont();

  // Paths and folders, compact, at the bottom.
  const float footer = o.y + size.y - Px(148, s);
  dl->AddLine(ImVec2(o.x, footer), ImVec2(o.x + size.x, footer), Color::BorderSubtle.U32());
  ImGui::SetCursorScreenPos(ImVec2(o.x + Px(28, s), footer + Px(14, s)));
  auto path_row = [&](const char *name, const fs::path &value, void (*change)(), bool enabled) {
    ImGui::PushID(name);
    push(Font::Meta);
    ImGui::PushStyleColor(ImGuiCol_Text, Color::TextSecondary.Vec4());
    ImGui::TextUnformatted(name);
    ImGui::PopStyleColor();
    ImGui::SameLine(Px(130, s));
    const float button_w = Px(84, s);
    const float avail = ImGui::GetContentRegionAvail().x - button_w - Px(8, s);
    const std::string v = value.empty() ? std::string(tr("Not set", "Не задано")) : text(value.wstring());
    ImGui::PushStyleColor(ImGuiCol_Text, (value.empty() ? Color::AccentAmber : Color::TextPrimary).Vec4());
    ImGui::PushClipRect(ImGui::GetCursorScreenPos(),
                        ImVec2(ImGui::GetCursorScreenPos().x + avail, ImGui::GetCursorScreenPos().y + Px(30, s)), true);
    ImGui::TextUnformatted(v.c_str());
    ImGui::PopClipRect();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", v.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine((ImGui::GetWindowSize().x - ImGui::GetStyle().WindowPadding.x) - button_w);
    ImGui::BeginDisabled(!enabled);
    if (ImGui::Button(tr("Change...", "Изменить..."), ImVec2(button_w, 0))) change();
    ImGui::EndDisabled();
    ImGui::PopFont();
    ImGui::PopID();
  };
  const bool idle = !launch.busy() && !game_running;
  path_row(tr("Game", "Игра"), app.game_path, choose_game, idle);
  path_row(tr("Mod loader", "Загрузчик"), app.loader_path, choose_loader, idle);
  push(Font::Meta);
  if (ImGui::Button(tr("Replays folder", "Папка записей"))) open_path(app.replays);
  ImGui::SameLine();
  if (ImGui::Button(tr("Logs", "Журналы"))) open_path(app.logs);
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text, Color::TextMuted.Vec4());
  ImGui::SetCursorPosX((ImGui::GetWindowSize().x - ImGui::GetStyle().WindowPadding.x) -
                       ImGui::CalcTextSize(tr("In game: F4 Theater Mode   F5 record   F6 stop   Space play/pause",
                                              "В игре: F4 Theater Mode   F5 запись   F6 стоп   Пробел пуск/пауза")).x);
  ImGui::TextUnformatted(tr("In game: F4 Theater Mode   F5 record   F6 stop   Space play/pause", "В игре: F4 Theater Mode   F5 запись   F6 стоп   Пробел пуск/пауза"));
  ImGui::PopStyleColor();
  ImGui::PopFont();

  ImGui::End();
  ImGui::PopStyleVar();
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
  ImGui_ImplWin32_EnableDpiAwareness();
  dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint({}, MONITOR_DEFAULTTOPRIMARY));
  WNDCLASSW wc{};
  wc.lpfnWndProc = launcher_proc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  wc.lpszClassName = L"EldenRingTheaterModeWindow";
  RegisterClassW(&wc);
  const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  RECT rect{0, 0, int(560 * dpi), int(440 * dpi)};
  AdjustWindowRect(&rect, style, FALSE);
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
  load_language();
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
    push(Font::Body);
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
