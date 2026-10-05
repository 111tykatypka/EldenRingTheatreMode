#include "in_game_replay.hpp"
#include "clock_helpers.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <sstream>

namespace in_game_replay {
namespace {
std::atomic<std::uint64_t> nonce{1};
double distance(game_control::Transform a,game_control::Transform b){double sum=0;for(unsigned i=0;i<3;++i)sum+=std::pow(double(a.position[i])-b.position[i],2);return std::sqrt(sum);}
}
bool Controller::active() const {return phase_==Phase::starting||phase_==Phase::playing||phase_==Phase::paused||phase_==Phase::finishing||phase_==Phase::restarting;}
void Controller::transition(Phase phase,const std::wstring& diagnostic){phase_=phase;diagnostic_=diagnostic;logger_("HOST_REPLAY_STATE="+std::to_string(static_cast<int>(phase))+" session="+std::to_string(session_));}
void Controller::fail(const std::wstring& diagnostic){control_.emergency_stop();if(player_)player_->stop();transition(Phase::error,diagnostic);player_=nullptr;}
game_control::Transform Controller::transform() const {const auto&s=player_->state();return {{s.position.x,s.position.y,s.position.z},{s.orientation.x,s.orientation.y,s.orientation.z,s.orientation.w}};}
bool Controller::play(replay::Player& player,std::uint64_t requested_limit,replay::Player::Clock::time_point now){
    if(phase_==Phase::paused&&player_==&player){player_->play(now);transition(Phase::playing,animation_?L"PLAYING — transforms + EXPERIMENTAL animation requests":L"PLAYING — transform only");tick(now);return active();}
    if(active())return false;
    if(xz_only_&&selected_actor_){fail(L"XZ diagnostic is player-only; disable selected-NPC mode");return false;}
    player_=&player;const auto remote=control_.state();const auto boot_ns=theater_clock::monotonic_ns();
    actors_=(characters_||selected_actor_)?player.reader().characters():std::vector<erplay::CharacterInfo>{};
    std::erase_if(actors_,[&](const auto&a){const bool unsupported=(std::uint32_t(a.registry.native_handle)>>28)!=1;if(unsupported)logger_("ACTOR_UNSUPPORTED_SELECTOR replay_id="+std::to_string(a.registry.id)+"; track captured but not applied");return unsupported;});
    if(selected_actor_){std::erase_if(actors_,[&](const auto&a){return a.registry.id!=selected_actor_;});
        if(actors_.size()!=1||!remote.nightly_supported){fail(L"Selected NPC track unavailable or matching nightly DLL required");return false;}}
    logger_("ACTOR_HOST_CONFIGURATION selected_only="+std::to_string(selected_actor_)+" enabled="+std::to_string(characters_)+" eligible_tracks="+std::to_string(actors_.size()));
    if((characters_||selected_actor_)&&!remote.actor_supported){fail(L"Actor replay requires matching tester DLL; disable experimental actors for player-only fallback");return false;}
    if(!remote.connected||!remote.ready||!remote.replay_supported){fail(L"ERROR: matching Phase5 DLL / Player FOUND required");return false;}
    if(remote.sample_timestamp_ns==0||remote.sample_timestamp_ns>boot_ns||boot_ns-remote.sample_timestamp_ns>500'000'000){fail(L"ERROR: live player state is stale");return false;}
    player_->seek(0);const auto first=transform();const auto d=distance(remote.live,first);
    std::ostringstream log;log<<"REPLAY_START_GUARD live=("<<remote.live.position[0]<<','<<remote.live.position[1]<<','<<remote.live.position[2]<<") first=("<<first.position[0]<<','<<first.position[1]<<','<<first.position[2]<<") delta=("<<first.position[0]-remote.live.position[0]<<','<<first.position[1]-remote.live.position[1]<<','<<first.position[2]-remote.live.position[2]<<") distance="<<d<<" policy=require-near-start map=UNKNOWN";logger_(log.str());
    if(!std::isfinite(d)){fail(L"ERROR: non-finite start displacement");return false;}
    const double dx=first.position[0]-remote.live.position[0],dz=first.position[2]-remote.live.position[2];
    const double dy=std::abs(double(first.position[1])-remote.live.position[1]);
    if(!selected_actor_&&(std::hypot(dx,dz)>1.0||dy>0.25)){
        fail(L"START BLOCKED: map UNKNOWN; return within 1.0 XZ / 0.25 Y of recorded start. No teleport performed.");return false;}
    if(xz_only_&&!remote.phase7_supported){fail(L"XZ diagnostic requires matching Phase7 DLL");return false;}
    limit_ns_=requested_limit?std::min(requested_limit,player.summary().duration_ns):player.summary().duration_ns;
    actor_log_ns_=0;session_=GetTickCount64()*1'000'000ULL+nonce.fetch_add(1);pause_on_start_=false;
    if(!control_.begin_replay(session_,first,player_->state().current_action,animation_&&!selected_actor_,selected_actor_!=0,xz_only_)){fail(L"ERROR: replay BEGIN refused (probe/STOP busy or unsupported DLL)");return false;}
    deadline_=now+std::chrono::seconds(2);transition(Phase::starting,L"STARTING — waiting for first game-thread write");
    logger_("REPLAY_START session="+std::to_string(session_)+" limit_ns="+std::to_string(limit_ns_)+" action_events="+std::to_string(player_->summary().action_event_count)+" animation_override="+std::to_string(animation_&&!selected_actor_));return true;
}
void Controller::stop(){control_.emergency_stop();if(player_)player_->stop();player_=nullptr;pause_on_start_=false;transition(Phase::inactive,L"INACTIVE — writes OFF / normal controls");logger_("REPLAY_STOP requested");}
void Controller::pause(replay::Player::Clock::time_point now){
    if(phase_==Phase::starting){pause_on_start_=true;return;}
    if(phase_!=Phase::playing||!player_)return;
    player_->pause(now);
    if(player_->state().timestamp_ns>=limit_ns_){finish(now);return;}
    transition(Phase::paused,L"PAUSED — holding transform (runtime validation required)");tick(now);
}
void Controller::restart(replay::Player& player,std::uint64_t limit,replay::Player::Clock::time_point now){
    if(!active()){play(player,limit,now);return;}
    stop();player_=&player;limit_ns_=limit;deadline_=now+std::chrono::seconds(2);transition(Phase::restarting,L"RESTARTING — waiting for STOP acknowledgement");
}
void Controller::finish(replay::Player::Clock::time_point now){
    player_->seek(limit_ns_);
    if(!control_.finish_replay(session_,limit_ns_,transform(),player_->state().current_action,animation_&&!selected_actor_,selected_actor_!=0,xz_only_)){fail(L"ERROR: final transform command refused");return;}
    deadline_=now+std::chrono::seconds(2);transition(Phase::finishing,L"FINISHING — final transform once, then writes OFF");
}
void Controller::tick(replay::Player::Clock::time_point now){
    if(!active()||!player_)return;
    const auto remote=control_.state();
    if(!remote.connected||!remote.ready){fail(L"ERROR: game/player connection lost; writes OFF");return;}
    if(phase_==Phase::restarting){
        if(!remote.pending&&remote.replay_phase==game_control::inactive){auto*p=player_;const auto limit=limit_ns_;phase_=Phase::inactive;play(*p,limit,now);}
        else if(now>=deadline_)fail(L"ERROR: STOP acknowledgement timed out; replay remains OFF");
        return;
    }
    if(remote.last_replay_session==session_&&remote.replay_phase==game_control::replay_error){
        fail(L"ERROR: DLL stopped replay; detail="+std::to_wstring(remote.replay_detail)+L" (see game log)");return;
    }
    if(phase_==Phase::starting){
        if(remote.session==session_&&remote.replay_phase==game_control::playing&&remote.applied_sequence){
            player_->play(now);transition(Phase::playing,animation_?L"PLAYING — transforms + EXPERIMENTAL animation requests":L"PLAYING — transform only");
            if(pause_on_start_){player_->pause(now);transition(Phase::paused,L"PAUSED — holding transform");}
        }else{if(now>=deadline_)fail(L"ERROR: first game-thread write was not acknowledged");return;}
    }
    if(phase_==Phase::finishing){
        if(remote.session==session_&&remote.replay_phase==game_control::finished){transition(Phase::finished,L"FINISHED — writes OFF / normal controls");player_=nullptr;logger_("REPLAY_FINISHED acknowledged");}
        else if(now>=deadline_)fail(L"ERROR: finish acknowledgement timed out; writes OFF");
        return;
    }
    if(phase_==Phase::playing){player_->advance(now);if(player_->state().timestamp_ns>=limit_ns_){finish(now);return;}}
    if(!control_.apply_replay(session_,player_->state().timestamp_ns,transform(),phase_==Phase::paused,player_->state().current_action,animation_&&!selected_actor_,selected_actor_!=0,xz_only_)){fail(L"ERROR: transform update refused; writes OFF");return;}
    if((characters_||selected_actor_)&&!send_characters())fail(L"ERROR: actor target update refused; all writes OFF");
}
bool Controller::send_characters(){
 const auto t=player_->state().timestamp_ns;std::vector<erplay::CharacterRecord> targets;targets.reserve(actors_.size());
 for(const auto& info:actors_){auto pair=player_->reader().character_bracket(info.registry.id,t);if(!pair)continue;const auto&[a,b]=*pair;auto row=info.registry;
   const double factor=b.timestamp_ns>a.timestamp_ns?std::clamp(double(t-a.timestamp_ns)/double(b.timestamp_ns-a.timestamp_ns),0.,1.):0.;
   for(unsigned i=0;i<3;++i)row.position[i]=float(a.position[i]+(b.position[i]-a.position[i])*factor);
   const auto q=replay::slerp({a.orientation[0],a.orientation[1],a.orientation[2],a.orientation[3]},{b.orientation[0],b.orientation[1],b.orientation[2],b.orientation[3]},factor);
   row.orientation={q.x,q.y,q.z,q.w};row.action=a.action;targets.push_back(row);
 }
 const auto now=theater_clock::monotonic_ns();
 if(now>=actor_log_ns_){
   logger_("ACTOR_HOST_FRAME session="+std::to_string(session_)+" replay_ns="+std::to_string(t)+" selected_only="+std::to_string(selected_actor_)+" eligible="+std::to_string(actors_.size())+" interpolated_targets="+std::to_string(targets.size()));
   if(selected_actor_&&!targets.empty()){const auto&r=targets.front();logger_("ACTOR_HOST_TARGET session="+std::to_string(session_)+" replay_id="+std::to_string(r.id)+" native_handle="+std::to_string(r.native_handle)+" entity="+std::to_string(r.entity_id)+" npc="+std::to_string(r.npc_param)+" raw_type="+std::to_string(r.character_type)+" requested_xyz="+std::to_string(r.position[0])+","+std::to_string(r.position[1])+","+std::to_string(r.position[2]));}
   actor_log_ns_=now+1'000'000'000;
 }
 return control_.actor_targets(session_,t,targets,phase_==Phase::paused);
}
}
