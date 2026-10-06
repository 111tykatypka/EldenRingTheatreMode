#pragma once
#include "editor_backend.hpp"
#include "editor_math.hpp"
namespace editor {
// Called immediately after the timeline canvas item.
void zoom_timeline_item(TimeView&, double duration);
void draw(const theater::Snapshot&,const theater::PlaybackView&,const game_control::State&,const game_launcher::State&);
void dispatch();
void load_settings();void save_settings();
}
