#pragma once
#include <cstdint>
namespace game_wind {
struct View { bool enabled=false; float strength=1; int status=0; std::uint32_t grass_rows=0,asset_rows=0;
 bool inspect=false;int probe_status=0;std::uint32_t native_slots=0,observed_records=0; };
View view();
void configure(bool enabled,float strength);
void inspect(bool enabled);
}
// Requests cross the UI/game-thread boundary; no native pointers cross it.
extern "C" int tm_wind_request(float* strength);
extern "C" void tm_wind_report(int status,std::uint32_t grass,std::uint32_t assets);
extern "C" void tm_wind_disable();
extern "C" void tm_wind_inspect_tick(int allowed);
