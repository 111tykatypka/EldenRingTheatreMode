#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace native_lights {
struct View {bool enabled=false,available=false,faulted=false,shadows=false,shadow_available=false;unsigned rendered=0;std::uint64_t created=0,removed=0,lock_skips=0;std::string status;std::vector<std::uint64_t> submitted_ids;};
View view();
void enable(bool value);
void shadows(bool value);
void host_connected(bool value);
}
extern "C" void tm_native_lights_tick(int active);
extern "C" void tm_native_lights_disable();
