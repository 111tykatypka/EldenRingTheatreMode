#include "game_control.hpp"
#include <cstring>

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
    command.timestamp_ns=GetTickCount64()*1'000'000ULL;
    return transfer(pipe,event,&command,sizeof(command),true)&&transfer(pipe,event,&reply,sizeof(reply),false)&&
        reply.magic_value==magic&&reply.version==1&&reply.kind==status&&reply.state<=error&&reply.flags<=1&&reply.sequence<=command.sequence;
}
}
Client::~Client(){close();}
void Client::start(std::atomic<DWORD>& pid){worker_=std::thread([this,&pid]{run(pid);});}
State Client::state() const {std::lock_guard lock(mutex_);return state_;}
bool Client::nudge(){std::lock_guard lock(mutex_);if(!state_.connected||!state_.ready||state_.pending||state_.phase==armed||state_.phase==observing)return false;
    int empty=0;if(!pending_.compare_exchange_strong(empty,1))return false;state_.pending=true;wake_.notify_one();return true;}
void Client::emergency_stop(){pending_.store(2);wake_.notify_one();}
void Client::close(){if(!worker_.joinable())return;emergency_stop();shutting_down_=true;wake_.notify_one();worker_.join();}
void Client::disconnected(const std::wstring& reason){pending_=0;std::lock_guard lock(mutex_);state_={};state_.diagnostic=reason;}
void Client::run(std::atomic<DWORD>& sample_pid){
    HANDLE pipe=INVALID_HANDLE_VALUE;HANDLE event=CreateEventW(nullptr,TRUE,FALSE,nullptr);std::uint64_t sequence=0;DWORD server_pid=0;
    if(!event){disconnected(L"Control event creation failed");return;}
    auto send=[&](std::uint16_t kind){Packet command;command.kind=kind;command.sequence=++sequence;if(kind==probe_nudge)command.position[0]=0.5f;
        Packet reply;if(!exchange(pipe,event,command,reply))return false;
        std::lock_guard lock(mutex_);state_.connected=true;state_.ready=(reply.flags&1)!=0;state_.pending=false;
        state_.phase=reply.state;state_.detail=reply.detail;state_.command_sequence=reply.sequence;state_.diagnostic=L"";return true;};
    while(true){
        if(shutting_down_.load()){if(pipe!=INVALID_HANDLE_VALUE)send(stop);break;}
        if(pipe==INVALID_HANDLE_VALUE){
            const auto pid=sample_pid.load();
            if(pid){pipe=CreateFileW(pipe_.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
                if(pipe!=INVALID_HANDLE_VALUE){
                    if(!GetNamedPipeServerProcessId(pipe,&server_pid)||server_pid!=pid){CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;disconnected(L"Control/sample pipe process mismatch");}
                    else if(!send(hello)){CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;disconnected(L"Control handshake failed");}
                }
            }
        }else if(sample_pid.load()!=server_pid){send(stop);CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;disconnected(L"Game sample connection lost; probe OFF");}
        else{const int request=pending_.exchange(0);const auto kind=request==2?stop:request==1?probe_nudge:heartbeat;
            if(!send(kind)){CloseHandle(pipe);pipe=INVALID_HANDLE_VALUE;disconnected(L"Control IPC disconnected; probe OFF");}}
        std::unique_lock lock(wake_mutex_);wake_.wait_for(lock,std::chrono::milliseconds(100),[&]{return shutting_down_.load()||pending_.load()!=0;});
    }
    if(pipe!=INVALID_HANDLE_VALUE)CloseHandle(pipe);CloseHandle(event);disconnected(L"Control closed; probe OFF");
}
}
