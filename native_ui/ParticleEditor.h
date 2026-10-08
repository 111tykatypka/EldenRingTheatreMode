#pragma once

#include "CameraTrack.h"
#include <cstdint>
#include <string>
#include <vector>

namespace particle_editor {
enum class Category : unsigned { Ambient, Environment, Weather, Fire, Smoke, Magic, Combat, Water, Custom };
struct Preset { int id = 0; Category category = Category::Ambient; const char* name = ""; };
struct Emitter {
    std::uint64_t id = 0; int preset = 0; std::string name; cinematic::State transform;
    bool enabled = true, loop = true; float duration_seconds = 1.0f, repeat_seconds = 0.0f, scale = 1.0f, intensity = 1.0f;
};
struct View { std::vector<Emitter> emitters; std::uint64_t selected = 0; std::string status; };
const Preset* presets(std::size_t& count); View view(); bool snapshot(View& out);
void create(int preset, const cinematic::State& camera); void select(std::uint64_t id); void edit(Emitter emitter);
void remove(std::uint64_t id); void clear(); void save(); void load();
} // namespace particle_editor
