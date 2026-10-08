#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "CameraTrack.h"
namespace native_particles {
struct View {
    bool available=false, dispatcher_verified=false, spawn_supported=false;
    std::uint64_t inspections=0;
    std::uint32_t debug_effect_id=0;
    float camera_distance=0;
    std::vector<std::uint32_t> loaded_effect_ids;
    std::string status="Native VFX not inspected";
};
View view();
void inspect();
struct PreviewView { bool active=false, faulted=false; std::uint32_t effect_id=0; std::string status="Native preview off"; };
PreviewView preview_view();
bool preview(std::uint32_t effect_id, const cinematic::State& transform);
void stop_preview();
}
extern "C" int tm_particles_inspection_requested();
extern "C" void tm_particles_inspect(int active, std::uintptr_t manager);
extern "C" int tm_particles_preview_requested();
extern "C" void tm_particles_preview_tick(int active, std::uintptr_t manager);
