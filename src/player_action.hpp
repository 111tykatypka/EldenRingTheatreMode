#pragma once
#include <cstdint>
#include <cmath>

namespace erplay {
enum class PlayerAction:std::uint32_t {unknown=0,idle,walk,run,sprint,turn,jump,fall,land,roll,backstep};
enum ActionFlags:std::uint32_t {animation_valid=1,time_valid=2,rate_valid=4,idle_id_match=8};
struct ActionState {
    PlayerAction action{PlayerAction::unknown};std::uint32_t flags{};
    std::uint64_t raw_action_bits{};std::int32_t animation_id{-1};
    float animation_time{},animation_length{},playback_rate{};
    bool valid()const {
        if(static_cast<std::uint32_t>(action)>10||flags>15)return false;
        if((flags&animation_valid)&&animation_id<0)return false;
        if(!(flags&animation_valid)&&animation_id!=-1)return false;
        if(!std::isfinite(animation_time)||!std::isfinite(animation_length)||!std::isfinite(playback_rate))return false;
        if((flags&time_valid)&&(!(flags&animation_valid)||animation_time<0||animation_length<=0||animation_time>animation_length+1.0f))return false;
        if((flags&rate_valid)&&(playback_rate<0||playback_rate>10))return false;
        return true;
    }
    bool same_action(const ActionState& b)const{return action==b.action&&flags==b.flags&&raw_action_bits==b.raw_action_bits&&animation_id==b.animation_id&&playback_rate==b.playback_rate;}
};
static_assert(sizeof(ActionState)==32);
struct ActionEvent {std::uint64_t timestamp_ns{};ActionState state;};
inline const wchar_t* action_name(PlayerAction a){constexpr const wchar_t* names[]{L"Unknown",L"Idle (runtime default ID)",L"Walk",L"Run",L"Sprint",L"Turn",L"Jump",L"Fall",L"Land",L"Roll",L"Backstep"};return names[static_cast<std::uint32_t>(a)<=10?static_cast<std::uint32_t>(a):0];}
}
