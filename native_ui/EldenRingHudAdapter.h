#pragma once
namespace game_hud {
bool ready();bool requested();void request(bool hide);void focused(bool value);
}
extern "C" int tm_hud_initialize();
extern "C" void tm_hud_game_context(int active);
