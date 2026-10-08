#include "replay_player.hpp"
#include "../shared/TheaterTimescale.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <bit>
#include <string_view>
#include <optional>

namespace replay {
namespace {
erplay::Quaternion normalize(erplay::Quaternion q) {
    const double n=std::sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w);
    if(!(n>1e-12&&std::isfinite(n))) return {0,0,0,1};
    return {float(q.x/n),float(q.y/n),float(q.z/n),float(q.w/n)};
}
// Decode only named schema fields. Never infer an action from movement velocity.
std::optional<std::uint32_t> captured_word(const erplay::CaptureRecord& record,unsigned track,std::string_view name,unsigned component=0) {
    const auto* definition=erplay::capture_track(track);if(!definition)return {};
    for(const auto& field:erplay::capture_fields)if(field.track==track&&name==field.name&&component<field.words) {
        const auto local=field.offset-definition->begin+component;
        if(record.available(local))return record.values[local];return {};
    }
    return {};
}
std::optional<erplay::ActionState> dense_action(const erplay::Reader& reader,std::uint64_t timestamp) {
    const auto animation=reader.capture_at(6,timestamp);if(!animation)return {};
    erplay::ActionState result;const auto index=captured_word(*animation,6,"read_idx");
    if(!index||*index>=10)return result;
    const auto prefix="queue_"+std::to_string(*index);
    const auto id=captured_word(*animation,6,prefix+"_anim_id");
    const auto time=captured_word(*animation,6,prefix+"_play_time");
    const auto length=captured_word(*animation,6,prefix+"_anim_length");
    const auto rate=captured_word(*animation,6,"animation_speed");
    if(id&&std::bit_cast<std::int32_t>(*id)>=0){result.animation_id=std::bit_cast<std::int32_t>(*id);result.flags|=erplay::animation_valid;}
    if(time&&length){const float phase=std::bit_cast<float>(*time),duration=std::bit_cast<float>(*length);
        if((result.flags&erplay::animation_valid)&&std::isfinite(phase)&&std::isfinite(duration)&&phase>=0&&duration>0&&phase<=duration+1){result.animation_time=phase;result.animation_length=duration;result.flags|=erplay::time_valid;}}
    if(rate){const float speed=std::bit_cast<float>(*rate);if(std::isfinite(speed)&&speed>=0&&speed<=10){result.playback_rate=speed;result.flags|=erplay::rate_valid;}}
    if(const auto actions=reader.capture_at(7,timestamp)) {
        const auto low=captured_word(*actions,7,"action_requests",0),high=captured_word(*actions,7,"action_requests",1);
        if(low&&high)result.raw_action_bits=std::uint64_t(*low)|(std::uint64_t(*high)<<32);
    }
    return result;
}
std::uint64_t ns_between(Player::Clock::time_point a,Player::Clock::time_point b) {
    const auto d=std::chrono::duration_cast<std::chrono::nanoseconds>(b-a).count();
    return d>0?static_cast<std::uint64_t>(d):0;
}
}
erplay::Quaternion slerp(erplay::Quaternion a,erplay::Quaternion b,double t) {
    a=normalize(a); b=normalize(b); t=std::clamp(t,0.0,1.0);
    double dot=double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z+double(a.w)*b.w;
    if(dot<0){dot=-dot;b={-b.x,-b.y,-b.z,-b.w};}
    if(dot>0.9995) return normalize({float(a.x+t*(b.x-a.x)),float(a.y+t*(b.y-a.y)),float(a.z+t*(b.z-a.z)),float(a.w+t*(b.w-a.w))});
    dot=std::clamp(dot,-1.0,1.0); const double theta=std::acos(dot),sin_theta=std::sin(theta);
    if(std::abs(sin_theta)<1e-12)return a;
    const double wa=std::sin((1.0-t)*theta)/sin_theta,wb=std::sin(t*theta)/sin_theta;
    return normalize({float(wa*a.x+wb*b.x),float(wa*a.y+wb*b.y),float(wa*a.z+wb*b.z),float(wa*a.w+wb*b.w)});
}
Player::Player(const std::filesystem::path& file) {
    state_.status=Status::loading;
    reader_=std::make_unique<erplay::Reader>(file);
    if(reader_->summary().metadata.game_version!="2.7.0.0") {state_.status=Status::error;throw std::runtime_error("incompatible replay game version");}
    if(reader_->summary().metadata.mod_version!="0.2.0"&&reader_->summary().metadata.mod_version!="0.3.0"&&reader_->summary().metadata.mod_version!="0.6.0"&&reader_->summary().metadata.mod_version!="0.7.0-fidelity1") {state_.status=Status::error;throw std::runtime_error("incompatible replay mod version");}
    if(reader_->summary().sample_count==0) {state_.status=Status::error;throw std::runtime_error("replay contains no samples");}
    state_.status=Status::ready; update_state();
}
const erplay::Summary& Player::summary() const noexcept{return reader_->summary();}
void Player::update_state() {
    const auto [a,b]=reader_->bracket(clock_ns_); state_.timestamp_ns=clock_ns_;
    state_.sample_index=reader_->lower_sample(clock_ns_); const auto span=b.replay_time_ns-a.replay_time_ns;
    const double t=span?double(clock_ns_-a.replay_time_ns)/double(span):0.0;
    state_.position={float(a.position.x+t*(b.position.x-a.position.x)),float(a.position.y+t*(b.position.y-a.position.y)),float(a.position.z+t*(b.position.z-a.position.z))};
    state_.orientation=slerp(a.orientation,b.orientation,t);
    const auto& events=reader_->action_events();state_.has_action=false;state_.current_action={};
    if(!events.empty()){if(!action_cursor_valid_||clock_ns_<previous_action_clock_){const auto it=std::upper_bound(events.begin(),events.end(),clock_ns_,[](std::uint64_t ns,const erplay::ActionEvent&e){return ns<e.timestamp_ns;});action_cursor_=static_cast<std::size_t>(it-events.begin());action_cursor_valid_=true;}else{while(action_cursor_<events.size()&&events[action_cursor_].timestamp_ns<=clock_ns_)++action_cursor_;}if(action_cursor_){state_.has_action=true;state_.current_action=events[action_cursor_-1].state;}}
    state_.dense_action=false;
    if(summary().capture_record_counts[1]) {
        state_.current_action={};state_.has_action=false;
        if(auto captured=dense_action(*reader_,clock_ns_)){state_.current_action=*captured;state_.has_action=(captured->flags&erplay::animation_valid)!=0;state_.dense_action=true;}
    }
    previous_action_clock_=clock_ns_;
}
void Player::play(Clock::time_point now) {
    if(clock_ns_>=summary().duration_ns){clock_ns_=0;fractional_ns_=0;}
    anchor_=now; state_.status=Status::playing; update_state();
}
void Player::pause(Clock::time_point now) { if(state_.status==Status::playing){advance(now);if(state_.status==Status::playing)state_.status=Status::paused;} }
void Player::stop(){action_cursor_valid_=false;clock_ns_=0;fractional_ns_=0;state_.status=Status::stopped;update_state();}
void Player::restart(Clock::time_point now){action_cursor_valid_=false;clock_ns_=0;fractional_ns_=0;anchor_=now;state_.status=Status::playing;update_state();}
void Player::set_timescale(double value,Clock::time_point now) {
    if(!theater_timescale::valid(value))throw std::invalid_argument("invalid Theater timescale");
    const auto was=state_.status==Status::playing;if(was)advance(now);state_.timescale=value;if(was)anchor_=now;
}
void Player::seek(std::uint64_t t) {action_cursor_valid_=false;state_.status=Status::seeking;clock_ns_=std::min(t,summary().duration_ns);fractional_ns_=0;update_state();state_.status=Status::paused;}
void Player::step(int direction) {
    if(direction==0)return;state_.status=Status::paused;fractional_ns_=0;
    if(direction>0&&state_.sample_index+1<summary().sample_count)clock_ns_=reader_->sample(state_.sample_index+1).replay_time_ns;
    else if(direction<0&&state_.sample_index>0)clock_ns_=reader_->sample(state_.sample_index-1).replay_time_ns;
    update_state();
}
void Player::advance(Clock::time_point now) {
    if(state_.status!=Status::playing)return;
    const long double delta=static_cast<long double>(ns_between(anchor_,now))*state_.timescale+fractional_ns_;
    const auto scaled=delta>=static_cast<long double>(UINT64_MAX)?UINT64_MAX:static_cast<std::uint64_t>(delta);
    fractional_ns_=delta>=static_cast<long double>(UINT64_MAX)?0:delta-static_cast<long double>(scaled);
    const auto duration=summary().duration_ns;
    if(!duration){clock_ns_=0;state_.status=Status::paused;}
    else {
        const auto remaining=duration-clock_ns_;
        if(scaled>=remaining){clock_ns_=(scaled-remaining)%duration;action_cursor_valid_=false;}
        else clock_ns_+=scaled;
    }
    anchor_=now;update_state();
}
void BookmarkStore::load(){timestamps_.clear();std::ifstream in(path_,std::ios::binary);if(!in)return;std::uint64_t t{};while(in>>t)timestamps_.push_back(t);if(!in.eof())throw std::runtime_error("invalid bookmark sidecar");std::sort(timestamps_.begin(),timestamps_.end());timestamps_.erase(std::unique(timestamps_.begin(),timestamps_.end()),timestamps_.end());}
void BookmarkStore::save() const {if(!path_.parent_path().empty())std::filesystem::create_directories(path_.parent_path());auto temp=path_;temp+=L".tmp";{std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("cannot write bookmark sidecar");for(auto t:timestamps_)out<<t<<"\n";if(!out)throw std::runtime_error("failed writing bookmark sidecar");}std::error_code ec;std::filesystem::rename(temp,path_,ec);if(ec){std::filesystem::remove(path_,ec);ec.clear();std::filesystem::rename(temp,path_,ec);}if(ec)throw std::runtime_error("cannot finalize bookmark sidecar");}
void BookmarkStore::add(std::uint64_t t){timestamps_.push_back(t);std::sort(timestamps_.begin(),timestamps_.end());timestamps_.erase(std::unique(timestamps_.begin(),timestamps_.end()),timestamps_.end());}
void BookmarkStore::erase(std::size_t i){if(i>=timestamps_.size())throw std::out_of_range("bookmark index out of range");timestamps_.erase(timestamps_.begin()+static_cast<std::ptrdiff_t>(i));}
}
