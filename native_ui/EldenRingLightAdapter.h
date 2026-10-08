#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace game_lights {
constexpr unsigned page_size=32;
struct Row { std::uintptr_t address=0; std::uint32_t id=0; int type=0; bool readable=false, spatial_valid=false; float spatial[4]{}; };
struct View { bool monitoring=false, available=false; unsigned collection=0, page=0, rows=0; std::uint64_t count[2]{}, generation=0, scans=0, sampled_ms=0; std::uintptr_t manager=0; std::array<Row,page_size> lights{}; std::string diagnostic="Inspection off; no lighting modified"; };
View view();
void request_scan();
void monitor(bool enabled);
void page(unsigned collection,unsigned index);
void host_connected(bool connected);
}
extern "C" void tm_lights_tick(int active);
