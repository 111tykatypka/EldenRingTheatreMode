#pragma once
namespace theater::ingame_editor {void start(const wchar_t* test_endpoint=nullptr);void shutdown();void poll();
// True while the in-game overlay polls the pipe (it does so every 50 ms when connected).
bool overlay_connected();}
