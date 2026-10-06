#include "ingame_editor_server.hpp"
#include "editor_backend.hpp"
#include "TheaterUiProtocol.h"
#include <deque>
namespace theater::ingame_editor {
namespace {std::jthread worker;std::mutex mutex;std::deque<theater_ui::Request> commands;theater_ui::Snapshot cached;std::atomic_bool running{};
bool transfer(HANDLE h,void*p,DWORD n,bool write){auto*c=static_cast<char*>(p);while(n){DWORD got=0;if(!(write?WriteFile(h,c,n,&got,nullptr):ReadFile(h,c,n,&got,nullptr))||!got)return false;c+=got;n-=got;}return true;}
void run(std::wstring endpoint){while(running){HANDLE h=CreateNamedPipeW(endpoint.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_REJECT_REMOTE_CLIENTS,1,sizeof(cached),sizeof(theater_ui::Request),0,nullptr);
 if(h==INVALID_HANDLE_VALUE){Sleep(100);continue;}
 if(ConnectNamedPipe(h,nullptr)||GetLastError()==ERROR_PIPE_CONNECTED){ULONG pid=0;std::uint64_t last=0;
 if(GetNamedPipeClientProcessId(h,&pid)&&pid&&pid==app.game_pid.load())while(running){theater_ui::Request r;if(!transfer(h,&r,sizeof(r),false)||!theater_ui::valid(r,last))break;last=r.sequence;
 theater_ui::Snapshot reply;{std::lock_guard lock(mutex);if(commands.size()>=32)break;if(r.command!=theater_ui::poll)commands.push_back(r);reply=cached;}reply.sequence=r.sequence;
 if(app.window)PostMessageW(app.window,WM_APP+77,0,0);
 if(!transfer(h,&reply,sizeof(reply),true))break;}}
 DisconnectNamedPipe(h);CloseHandle(h);
}}
}
void start(const wchar_t* test_endpoint){running=true;worker=std::jthread(run,std::wstring(test_endpoint?test_endpoint:theater_ui::pipe));}
void shutdown(){running=false;if(worker.joinable()){CancelSynchronousIo(worker.native_handle());worker.join();}}
// Called on the host UI thread, never on Present or a game callback.
void poll(){std::deque<theater_ui::Request> batch;{std::lock_guard lock(mutex);batch.swap(commands);}
 static std::uint32_t offset=0;
 for(const auto&r:batch){switch(r.command){
 case theater_ui::play:play_replay();break;case theater_ui::pause:pause_replay();break;case theater_ui::stop:emergency_stop();break;case theater_ui::restart:restart_replay();break;
 case theater_ui::seek:seek_replay(r.value);break;case theater_ui::previous:step_replay(-1);break;case theater_ui::next:step_replay(1);break;
 case theater_ui::speed:{std::lock_guard lock(app.replay_mutex);if(app.replay_player)app.replay_player->set_speed(double(r.value)/100.);break;}
 case theater_ui::select:if(!app.game_replay->active())app.selected_replay_actor=r.value;break;
 case theater_ui::page:offset=static_cast<std::uint32_t>(std::min<std::uint64_t>(r.value,UINT32_MAX));break;
 // Same recorder path as F5 and the host Start button; post_command refuses while a replay is active.
 case theater_ui::record_start:post_command(Command::start);break;case theater_ui::record_stop:post_command(Command::stop);break;
 default:break;}}
 const auto view=playback_view();const auto remote=app.control.state();theater_ui::Snapshot s;
 s.loaded=view.loaded;s.active=view.active;s.phase=static_cast<std::uint32_t>(view.phase);s.time_ns=view.state.timestamp_ns;s.duration_ns=view.summary.duration_ns;s.playback_speed=view.state.speed;
 s.selected=app.selected_replay_actor;s.connected=remote.connected;s.player_found=remote.ready;std::copy(remote.live.position.begin(),remote.live.position.end(),s.live_position);
 auto diagnostic=game_launcher::utf8(view.diagnostic);memcpy(s.diagnostic,diagnostic.data(),std::min(diagnostic.size(),sizeof(s.diagnostic)-1));
 s.total=static_cast<std::uint32_t>(app.character_views.size());s.offset=std::min(offset,s.total);s.count=std::min(16u,s.total-s.offset);
 {std::lock_guard lock(app.mutex);const auto rs=app.data.state;s.recording_state=rs==erplay::RecordingState::recording?theater_ui::record_recording:rs==erplay::RecordingState::paused?theater_ui::record_paused:rs==erplay::RecordingState::saving?theater_ui::record_saving:theater_ui::record_idle;s.recording_ns=app.data.active_ns;s.recording_samples=app.data.samples;}
 for(unsigned i=0;i<s.count;++i){const auto&r=app.character_views[s.offset+i].info.registry;s.actors[i]={r.id,r.native_handle,r.entity_id,r.character_type,r.npc_param,0};}
 {std::lock_guard lock(mutex);cached=s;}
}
}
