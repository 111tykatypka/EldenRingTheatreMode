#pragma once
#include <string>
#include <cstddef>
namespace game_timing {
void enable(bool enabled);
bool enabled();
std::string status();
}
extern "C" void tm_world_timing_tick(int active,double speed);
extern "C" int tm_world_timing_diagnostic(char*out,std::size_t size);
