#pragma once
#include "editor_backend.hpp"
namespace editor {
void draw(const theater::Snapshot&,const theater::PlaybackView&,const game_control::State&,const game_launcher::State&);
void dispatch();
void load_settings();void save_settings();
}
