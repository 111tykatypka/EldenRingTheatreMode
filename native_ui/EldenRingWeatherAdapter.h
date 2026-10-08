#pragma once
#include <string>
#include <cstddef>
namespace game_weather {
struct Preset { int id; const char* name; };
const Preset* presets(std::size_t& count);
struct View { bool available=false, enabled=false, applied=false; int current=-1, pending=-1, selected=1; std::string diagnostic; };
View view();
void select(int id);
void enable(bool enabled);
void host_connected(bool connected);
}
// Only the verified PostPhysics game task calls this; UI/IPC never write game memory.
extern "C" void tm_weather_tick(int active);
