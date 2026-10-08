#pragma once
#include <cstdint>
#include <string>
namespace native_lights {
struct View {bool enabled=false,available=false,faulted=false;unsigned rendered=0;std::uint64_t created=0,removed=0,lock_skips=0;std::string status;};
View view();
void enable(bool value);
void host_connected(bool value);
}
extern "C" void tm_native_lights_tick(int active);
extern "C" void tm_native_lights_disable();
