#include "game_control.hpp"
#include "clock_helpers.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace game_control {
namespace {
bool transfer(HANDLE pipe,HANDLE event,void* data,DWORD bytes,bool writing) {
    auto* cursor=static_cast<unsigned char*>(data);
    while(bytes){OVERLAPPED op{};op.hEvent=event;ResetEvent(event);DWORD count=0;
        const BOOL ok=writing?WriteFile(pipe,cursor,bytes,&count,&op):ReadFile(pipe,cursor,bytes,&count,&op);
        if(!ok){if(GetLastError()!=ERROR_IO_PENDING)return false;
            if(WaitForSingleObject(event,300)!=WAIT_OBJECT_0){CancelIoEx(pipe,&op);GetOverlappedResult(pipe,&op,&count,TRUE);return false;}
            if(!GetOverlappedResult(pipe,&op,&count,FALSE))return false;
        }
        if(count==0||count>bytes)return false;cursor+=count;bytes-=count;
    }return true;
}
bool exchange(HANDLE pipe,HANDLE event,Packet& command,Packet& reply) {
    if(!command.timestamp_ns)command.timestamp_ns=theater_clock::monotonic_ns();
    return transfer(pipe,event,&command,sizeof(command),true)&&transfer(pipe,event,&reply,sizeof(reply),false)&&
        reply.magic_value==magic&&reply.version==3&&reply.kind==status&&reply.state<=error&&reply.flags<=3&&reply.sequence==command.sequence&&reply.replay_state<=replay_error&&reply.applied_sequence<=reply.sequence;
}
}
Client::~Client(){close();}
void Client::start(std::atomic<DWORD>& pid){worker_=std::thread([this,&pid]{run(pid);});}
State Client::state() const {std::lock_guard lock(mutex_);return state_;}
bool Client::nudge(){std::lock_guard lock(mutex_);if(!state_.connected||!state_.ready||state_.pending||state_.phase==armed||state_.phase==observing||active_session_)return false;
    int empty=0;if(!pending_.compare_exchange_strong(empty,1))return false;state_.pending=true;wake_.notify_one();return true;}
bool Client::begin_replay(std::uint64_t session,Transform t,erplay::ActionState a,bool animation){return queue_replay(replay_begin,session,0,t,false,a,animation);}
bool Client::apply_replay(std::uint64_t session,std::uint64_t ns,Transform t,bool pause,erplay::ActionState a,bool animation){return queue_replay(replay_apply,session,ns,t,pause,a,animation);}
bool Client::finish_replay(std::uint64_t session,std::uint64_t ns,Transform t,erplay::ActionState a,bool animation){return queue_replay(replay_finish,session,ns,t,false,a,animation);}
bool Client::queue_replay(std::uint16_t kind,std::uint64_t session,std::uint64_t ns,Transform t,bool pause,erplay::ActionState a,bool animation){
    if(!a.valid())return false;
    double norm=0;for(auto v:t.position)if(!std::isfinite(v))return false;for(auto v:t.quaternion){if(!std::isfinite(v))return false;norm+=double(v)*v;}
    if(!session||std::abs(norm-1.0)>0.001)return false;
    std::lock_guard lock(mutex_);
    if(!state_.connected||!state_.ready||!state_.replay_supported||pending_.load()!=0||state_.phase==armed||state_.phase==observing)return false;
    if(kind==replay_begin){if(active_session_||state_.pending||state_.replay_phase==playing||state_.replay_phase==paused)return false;active_session_=session;}
    else if(active_session_!=session)return false;
    Packet p;p.timestamp_ns=theater_clock::monotonic_ns();p.kind=kind;p.session=session;p.replay_timestamp_ns=ns;p.replay_state=pause?paused:playing;p.action=a;p.flags=animation?1:0;
    std::copy(t.position.begin(),t.position.end(),p.position);std::copy(t.quaternion.begin(),t.quaternion.end(),p.quaternion);
    latest_request_=p;replay_pending_=true;state_.pending=true;wake_.notify_one();return true;
}
void Client::emergency_stop(){{std::lock_guard lock(mutex_);latest_request_.reset();active_session_=0;replay_pending_=false;state_.pending=true;pending_.store(2);}wake_.notify_one();}
void Client::close(){if(!worker_.joinable())return;emergency_stop();shutting_down_=true;wake_.notify_one();worker_.join();}
void Client::disconnected(const std::wstring& reason){std::lock_guard lock(mutex_);pending_=0;latest_request_.reset();active_session_=0;replay_pending_=false;state_={};state_.diagnostic=reason;}
void Client::run(std::atomic<DWORD>& sample_pid){
    HANDLE pipe=INVALID_HANDLE_VALUE;HANDLE event=CreateEventW(nullptr,TRUE,FALSE,nullptr);std::uint64_t sequence=0;DWORD server_pid=0;
    auto perf_start=std::chrono::steady_clock::now();std::uint64_t replay_sends=0;
    if(!event){disconnected(L"Control event creation failed");return;}
    auto send=[&](Packet command){command.sequence=++sequence;if(command.kind==probe_nudge)command.position[0]=0.5f;
        Packet reply;if(!exchange(pipe,event,command,reply))return false;
        if(reply.flags&1){double norm=0;for(auto v:reply.position)if(!std::isfinite(v))return false;for(auto v:reply.quaternion){if(!std::isfinite(v))return false;norm+=double(v)*v;}if(std::abs(norm-1.0)>0.01)return false;}
        std::lock_guard lock(mutex_);state_.connected=true;state_.ready=(reply.flags&1)!=0;state_.replay_supported=(reply.flags&2)!=0;
        if(command.kind==replay_begin||command.kind==replay_apply||command.kind==replay_finish)++replay_sends;
        const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-perf_start).count();
        if(elapsed>=1.0){state_.replay_send_hz=double(replay_sends)/elapsed;replay_sends=0;perf_start=std::chrono::steady_clock::now();}
        state_.pending=pending_.load()!=0||latest_request_.has_value();state_.phase=reply.state;state_.detail=reply.detail;state_.command_sequence=reply.sequence;state_.diagnostic=L"";
        state_.replay_phase=reply.replay_state;state_.replay_detail=reply.replay_detail;state_.session=reply.session;state_.applied_sequence=reply.applied_sequence;
        state_.replay_timestamp_ns=reply.replay_timestamp_ns;state_.sample_timestamp_ns=reply.timestamp_ns;
        std::copy(std::begin(reply.position),std::end(reply.position),state_.live.position.begin());std::copy(std::begin(reply.quaternion),std::end(reply.quaternion),state_.live.quaternion.begin());
        if(command.kind==replay_begin||command.kind==replay_apply||command.kind==replay_finish)state_.last_replay_session=command.session;
        if(reply.replay_state==finished&&reply.session==active_session_)active_session_=0;
        return true;};
    auto send_kind=[&](std::uint16_t kind){Packet p;p.kind=kind;return send(p);};
    while(true){
        if(shutting_down_.load()){if(pipe!=INVALID_HANDLE_VALUE)send_kind(stop);break;}
        if(pipe==INVALID_HANDLE_VALUE){
            const auto pid=sample_pid.load();
            if(pid){pipe=CreateFileW(pipe_.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
                if(pipe!=INVALID_HANDLE_VALUE){
                    if(!GetNamedPipeServerProcessId(pipe,&server_pid)||server_pid!=pid){CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;disconnected(L"Control/sample pipe process mismatch");}
                    else if(!send_kind(hello)){CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;disconnected(L"Control v3 handshake failed: use the matching Phase5 DLL and restart the game");}
                }
            }
        }else if(sample_pid.load()!=server_pid){send_kind(stop);CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;disconnected(L"Game sample connection lost; probe/replay OFF");}
        else{Packet request;request.kind=heartbeat;
            {std::lock_guard lock(mutex_);const int action=pending_.exchange(0);
                if(action==2){request.kind=stop;latest_request_.reset();active_session_=0;replay_pending_=false;}
                else if(action==1)request.kind=probe_nudge;
                else if(latest_request_){request=*latest_request_;latest_request_.reset();replay_pending_=false;}}
            if(!send(request)){CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;disconnected(L"Control IPC disconnected; probe/replay OFF");}}
        std::unique_lock lock(wake_mutex_);wake_.wait_for(lock,std::chrono::milliseconds(100),[&]{return shutting_down_.load()||pending_.load()!=0||replay_pending_.load();});
    }
    if(pipe!=INVALID_HANDLE_VALUE)CloseHandle(pipe);CloseHandle(event);disconnected(L"Control closed; probe/replay OFF");
}
}
