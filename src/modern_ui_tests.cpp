#include "imgui.h"
#include "imgui_internal.h"
#include "modern_ui.hpp"
#include <iostream>
int main() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto &io = ImGui::GetIO();
  // Regression: captured keyboard focus must not suppress Windows global hotkeys.
  io.WantCaptureKeyboard=true;
  for(auto entry:{std::pair<UINT,theater::Command>{1,theater::Command::start}}){
    if(!theater::handle_global_hotkey(entry.first))return 3;
    std::lock_guard lock(theater::app.commands_mutex);
    if(theater::app.commands.size()!=1||theater::app.commands.front()!=entry.second)return 4;
    theater::app.commands.pop();
  }
  if(theater::handle_global_hotkey(3)||theater::handle_global_hotkey(4)||theater::handle_global_hotkey(99))return 5;
  io.WantCaptureKeyboard=false;
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.DeltaTime = 1.f / 60;
  io.Fonts->AddFontDefault();
  unsigned char *pixels;
  int w, h;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
  auto folder = std::filesystem::temp_directory_path() /
                std::filesystem::path("TheaterModernUi_" +
                                      std::to_string(GetCurrentProcessId()));
  std::filesystem::create_directories(folder);
  theater::app.root = folder;
  auto file = folder / L"ui_fixture.erplay";
  {
    erplay::Metadata m;
    m.format_version = 3;
    erplay::Writer writer(file, m, 2);
    for (std::uint64_t i = 0; i < 10; ++i) {
      erplay::Sample sample;
      sample.index = i;
      sample.replay_time_ns = i * 1'000'000'000;
      sample.source_time_ns = sample.replay_time_ns;
      sample.position = {float(i), 0, float(i)};
      writer.append(sample);
      erplay::CharacterRecord r;
      r.id = 1;
      r.timestamp_ns = sample.replay_time_ns;
      r.position = {float(i), 0, 2};
      if (i == 0) {
        auto reg = r;
        reg.kind = erplay::CharacterKind::registry;
        writer.append_character(reg);
      }
      writer.append_character(r);
    }
    auto result = writer.finalize();
  }
  theater::open_replay(file);
  // UI actor cursors and playback reuse the same Reader stream; production
  // must serialize both paths using replay_mutex.
  std::atomic_bool read_failed{};
  std::jthread reader([&]{
    try{for(unsigned i=0;i<1000;++i){std::lock_guard lock(theater::app.replay_mutex);
      const auto index=i%10;auto sample=theater::app.replay_player->reader().sample(index);
      auto actor=theater::app.replay_player->reader().character_bracket(1,index*1'000'000'000ULL);
      if(!actor||sample.position.x!=float(index)||actor->first.position[0]!=float(index))read_failed=true;
    }}catch(...){read_failed=true;}
  });
  for(unsigned i=0;i<1000;++i)theater::refresh_character_cursor((i%10)*1'000'000'000ULL);
  reader.join();if(read_failed)return 2;
  for (auto size : {ImVec2{1280, 720}, ImVec2{1920, 1080}, ImVec2{3440, 1440},
                    ImVec2{7680, 2160}, ImVec2{800, 600}}) {
    io.DisplaySize = size;
    for (int frame = 0; frame < 3; ++frame) {
      ImGui::NewFrame();
      auto playback = theater::playback_view();
      playback.state.timestamp_ns =
          std::array<std::uint64_t, 3>{0, 8'900'000'000, 9'000'000'000}[frame];
      theater::refresh_character_cursor(playback.state.timestamp_ns);
      editor::draw({}, playback, {}, {});
      ImGui::Render();
      if (!ImGui::GetDrawData()->Valid)
        return 1;
    }
  }
  // Real ImGui wheel events on an overflowing child: both directions must
  // zoom, never scroll the canvas/parent, and require no modifier key.
  editor::TimeView view;
  view.fit(60);
  io.DisplaySize = {800,600};
  io.AddMousePosEvent(150,150);
  float child_scroll{}, parent_scroll{};
  auto wheel_frame = [&](float wheel) {
    if(wheel) io.AddMouseWheelEvent(0,wheel);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({20,20});
    ImGui::SetNextWindowSize({400,300});
    ImGui::Begin("Wheel regression",nullptr,ImGuiWindowFlags_NoSavedSettings);
    ImGui::BeginChild("Overflow",{0,0});
    ImGui::InvisibleButton("canvas",{300,1000});
    editor::zoom_timeline_item(view,60);
    child_scroll=ImGui::GetScrollY();
    ImGui::EndChild();
    parent_scroll=ImGui::GetScrollY();
    ImGui::End();
    ImGui::Render();
    return child_scroll==0 && parent_scroll==0;
  };
  wheel_frame(0); wheel_frame(0); wheel_frame(0);
  for(int i=0;i<10;++i) {
    const auto before=view.span;
    if(!wheel_frame(1)||view.span>=before) return 6;
    const auto zoomed=view.span;
    if(!wheel_frame(-1)||view.span<=zoomed||io.KeyShift) return 7;
  }
  // Shift scrolls vertically in both directions without changing time scale.
  io.AddKeyEvent(ImGuiMod_Shift,true);
  wheel_frame(0);
  const auto span=view.span, begin=view.begin;
  wheel_frame(-1); wheel_frame(0);
  const float down=child_scroll;
  if(down<=0 || parent_scroll!=0 || view.span!=span || view.begin!=begin) return 8;
  wheel_frame(1); wheel_frame(0);
  if(child_scroll>=down || child_scroll!=0 || view.span!=span) return 9;
  // Clamp at both ends, then restore ordinary zoom without a stuck modifier.
  for(int i=0;i<20;++i)wheel_frame(-1);
  wheel_frame(0);
  const float bottom=child_scroll;
  wheel_frame(-1);wheel_frame(0);
  if(bottom<=0 || child_scroll!=bottom || parent_scroll!=0 || view.span!=span) return 10;
  for(int i=0;i<20;++i)wheel_frame(1);
  wheel_frame(0);
  if(child_scroll!=0) return 11;
  io.AddKeyEvent(ImGuiMod_Shift,false);
  wheel_frame(0);wheel_frame(1);
  if(view.span>=span || child_scroll!=0) return 12;
  ImGui::DestroyContext();
  std::cout << "ImGui docking/render construction at five resolutions passed "
               "(no visual/runtime assertion)\n";
}
