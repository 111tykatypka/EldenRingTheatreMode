#include "imgui.h"
#include "modern_ui.hpp"
#include <iostream>
int main() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto &io = ImGui::GetIO();
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
  ImGui::DestroyContext();
  std::cout << "ImGui docking/render construction at five resolutions passed "
               "(no visual/runtime assertion)\n";
}
