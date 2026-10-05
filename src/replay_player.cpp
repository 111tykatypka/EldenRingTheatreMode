#include "replay_player.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace replay {
namespace {
erplay::Quaternion normalize(erplay::Quaternion q) {
    const double n=std::sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w);
    if(!(n>1e-12&&std::isfinite(n))) return {0,0,0,1};
    return {float(q.x/n),float(q.y/n),float(q.z/n),float(q.w/n)};
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
    if(reader_->summary().metadata.mod_version!="0.2.0") {state_.status=Status::error;throw std::runtime_error("incompatible replay mod version");}
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
}
void Player::play(Clock::time_point now) {
    if(clock_ns_>=summary().duration_ns)clock_ns_=0;
    anchor_=now; state_.status=Status::playing; update_state();
}
void Player::pause(Clock::time_point now) { if(state_.status==Status::playing){advance(now);if(state_.status==Status::playing)state_.status=Status::paused;} }
void Player::stop(){clock_ns_=0;state_.status=Status::stopped;update_state();}
void Player::restart(Clock::time_point now){clock_ns_=0;anchor_=now;state_.status=Status::playing;update_state();}
void Player::set_speed(double speed,Clock::time_point now) {
    if(!(speed==0.1||speed==0.25||speed==0.5||speed==1.0||speed==2.0||speed==4.0))throw std::invalid_argument("unsupported replay speed");
    const auto was=state_.status==Status::playing;if(was)advance(now);state_.speed=speed;if(was)anchor_=now;
}
void Player::seek(std::uint64_t t) {state_.status=Status::seeking;clock_ns_=std::min(t,summary().duration_ns);update_state();state_.status=Status::paused;}
void Player::step(int direction) {
    if(direction==0)return;state_.status=Status::paused;
    if(direction>0&&state_.sample_index+1<summary().sample_count)clock_ns_=reader_->sample(state_.sample_index+1).replay_time_ns;
    else if(direction<0&&state_.sample_index>0)clock_ns_=reader_->sample(state_.sample_index-1).replay_time_ns;
    update_state();
}
void Player::advance(Clock::time_point now) {
    if(state_.status!=Status::playing)return;
    const long double delta=static_cast<long double>(ns_between(anchor_,now))*state_.speed;
    const auto scaled=delta>=static_cast<long double>(UINT64_MAX)?UINT64_MAX:static_cast<std::uint64_t>(delta);
    clock_ns_=scaled>summary().duration_ns-clock_ns_?summary().duration_ns:clock_ns_+scaled;anchor_=now;update_state();
    if(clock_ns_>=summary().duration_ns)state_.status=Status::paused;
}
void BookmarkStore::load(){timestamps_.clear();std::ifstream in(path_,std::ios::binary);if(!in)return;std::uint64_t t{};while(in>>t)timestamps_.push_back(t);if(!in.eof())throw std::runtime_error("invalid bookmark sidecar");std::sort(timestamps_.begin(),timestamps_.end());timestamps_.erase(std::unique(timestamps_.begin(),timestamps_.end()),timestamps_.end());}
void BookmarkStore::save() const {if(!path_.parent_path().empty())std::filesystem::create_directories(path_.parent_path());auto temp=path_;temp+=L".tmp";{std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("cannot write bookmark sidecar");for(auto t:timestamps_)out<<t<<"\n";if(!out)throw std::runtime_error("failed writing bookmark sidecar");}std::error_code ec;std::filesystem::rename(temp,path_,ec);if(ec){std::filesystem::remove(path_,ec);ec.clear();std::filesystem::rename(temp,path_,ec);}if(ec)throw std::runtime_error("cannot finalize bookmark sidecar");}
void BookmarkStore::add(std::uint64_t t){timestamps_.push_back(t);std::sort(timestamps_.begin(),timestamps_.end());timestamps_.erase(std::unique(timestamps_.begin(),timestamps_.end()),timestamps_.end());}
void BookmarkStore::erase(std::size_t i){if(i>=timestamps_.size())throw std::out_of_range("bookmark index out of range");timestamps_.erase(timestamps_.begin()+static_cast<std::ptrdiff_t>(i));}
}
