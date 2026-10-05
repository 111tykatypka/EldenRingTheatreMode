#include "editor_math.hpp"
#include "erplay.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
void check(bool b) {
  if (!b)
    throw std::runtime_error("check failed");
}
template <class F> void rejects(F fn) {
  bool caught = false;
  try {
    fn();
  } catch (...) {
    caught = true;
  }
  check(caught);
}
int main() {
  try {
    auto root =
        std::filesystem::temp_directory_path() /
        std::filesystem::path(
            L"TheaterModern_\u0422\u0435\u0441\u0442_" +
            std::to_wstring(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    auto file = root / L"characters.erplay";
    erplay::Metadata metadata;
    metadata.format_version = 3;
    {
      erplay::Writer writer(file, metadata, 3);
      for (std::uint64_t t = 0; t < 100; ++t) {
        erplay::Sample s;
        s.index = t;
        s.replay_time_ns = t * 50'000'000;
        s.source_time_ns = s.replay_time_ns + 123;
        s.position.x = float(t);
        writer.append(s);
        for (std::uint64_t id = 1; id <= 3; ++id) {
          erplay::CharacterRecord r;
          r.id = id;
          r.timestamp_ns = s.replay_time_ns;
          r.position = {float(id), float(t), 0};
          if (t == 0) {
            auto reg = r;
            reg.kind = erplay::CharacterKind::registry;
            reg.entity_id = 123;
            reg.npc_param = 432;
            writer.append_character(reg);
          }
          writer.append_character(r);
          if (t == 99) {
            r.kind = erplay::CharacterKind::presence;
            writer.append_character(r);
          }
        }
      }
      auto summary = writer.finalize();
      check(summary.sample_count == 100 && summary.character_count == 3 &&
            summary.character_sample_count == 300);
    }
    erplay::Reader reader(file);
    check(reader.characters().size() == 3);
    check(reader.character_preview(1, 10).size() <= 11);
    check(reader.character_at(1, 50'000'000)->position[1] == 1);
    check(!reader.character_at(1, 4'950'000'000));
    check(!reader.character_at(999, 1));
    check(reader.sample(99).position.x == 99);
    erplay::CharacterValidator validator;
    erplay::CharacterRecord r;
    r.id = 7;
    rejects([&] { validator.accept(r); });
    r.kind = erplay::CharacterKind::registry;
    validator.accept(r);
    rejects([&] { validator.accept(r); });
    r.kind = erplay::CharacterKind::transform;
    r.position[0] = std::numeric_limits<float>::infinity();
    rejects([&] { validator.accept(r); });
    r.position[0] = 0;
    r.orientation = {0, 0, 0, 0};
    rejects([&] { validator.accept(r); });
    r.orientation = {0, 0, 0, 1};
    r.timestamp_ns = 5;
    validator.accept(r);
    r.timestamp_ns = 4;
    rejects([&] { validator.accept(r); });
    auto broken = root / L"corrupt.erplay";
    std::filesystem::copy_file(file, broken);
    {
      std::fstream f(broken, std::ios::in | std::ios::out | std::ios::binary);
      f.seekp(-50, std::ios::end);
      char c = 127;
      f.write(&c, 1);
    }
    rejects([&] { (void)erplay::validate(broken); });
    auto interrupted = root / L"interrupted.erplay";
    {
      erplay::Writer w(interrupted, metadata, 1);
      erplay::Sample sample;
      w.append(sample);
      erplay::CharacterRecord reg;
      reg.id = 1;
      reg.kind = erplay::CharacterKind::registry;
      w.append_character(reg);
      reg.kind = erplay::CharacterKind::transform;
      w.append_character(reg);
      sample.index = 1;
      sample.replay_time_ns = 50'000'000;
      sample.source_time_ns = 50'000'000;
      w.append(sample);
    }
    auto temp = interrupted;
    temp += L".tmp";
    auto recovered =
        erplay::recover_incomplete(temp, root / L"recovered.erplay");
    check(recovered.sample_count == 2 && recovered.character_count == 1);
    editor::TimeView time;
    time.fit(68.5);
    for (double p : {0., .5, .9, .99, 1.})
      check(std::abs(time.time(time.pixel(p * 68.5, 800), 800, 68.5) -
                     p * 68.5) < 1e-9);
    time.zoom(.25, .5, 68.5);
    check(time.begin >= 0 && time.begin + time.span <= 68.5);
    time.pan(1e8, 68.5);
    check(time.begin + time.span <= 68.5);
    time.fit(0);
    check(std::isfinite(time.time(0, 0, 0)));
    float a[]{0, 0, 0}, b[]{10000, 0, 0};
    check(editor::start_distance(a, b) ==
          10000); // warning magnitude, not a hard blocker
    std::cout << "character registry/transform/presence, CRC, recovery, "
                 "Unicode and timeline math passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
