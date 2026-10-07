#include "ingame_editor_server.hpp"
#include "editor_backend.hpp"
#include "TheaterUiProtocol.h"
#include "replay_library.hpp"
#include <chrono>
#include <deque>
namespace theater::ingame_editor {
namespace {std::jthread worker;std::mutex mutex;std::deque<theater_ui::Request> commands;theater_ui::Snapshot cached;std::atomic_bool running{};std::atomic<ULONGLONG> last_request{};
bool transfer(HANDLE h,void*p,DWORD n,bool write){auto*c=static_cast<char*>(p);while(n){DWORD got=0;if(!(write?WriteFile(h,c,n,&got,nullptr):ReadFile(h,c,n,&got,nullptr))||!got)return false;c+=got;n-=got;}return true;}
void run(std::wstring endpoint){while(running){HANDLE h=CreateNamedPipeW(endpoint.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_REJECT_REMOTE_CLIENTS,1,sizeof(cached),sizeof(theater_ui::Request),0,nullptr);
 if(h==INVALID_HANDLE_VALUE){Sleep(100);continue;}
 if(ConnectNamedPipe(h,nullptr)||GetLastError()==ERROR_PIPE_CONNECTED){ULONG pid=0;std::uint64_t last=0;
 if(GetNamedPipeClientProcessId(h,&pid)&&pid&&pid==app.game_pid.load())while(running){theater_ui::Request r;if(!transfer(h,&r,sizeof(r),false)||!theater_ui::valid(r,last))break;last=r.sequence;last_request=GetTickCount64();
 theater_ui::Snapshot reply;{std::lock_guard lock(mutex);if(commands.size()>=32)break;if(r.command!=theater_ui::poll)commands.push_back(r);reply=cached;}reply.sequence=r.sequence;
 if(app.window)PostMessageW(app.window,WM_APP+77,0,0);
 if(!transfer(h,&reply,sizeof(reply),true))break;}}
 DisconnectNamedPipe(h);CloseHandle(h);
}}
}
bool overlay_connected(){const auto t=last_request.load();return t&&GetTickCount64()-t<1500;}
void start(const wchar_t* test_endpoint){running=true;worker=std::jthread(run,std::wstring(test_endpoint?test_endpoint:theater_ui::pipe));}
void shutdown(){running=false;if(worker.joinable()){CancelSynchronousIo(worker.native_handle());worker.join();}}
// Called on the host UI thread, never on Present or a game callback.
void poll(){std::deque<theater_ui::Request> batch;{std::lock_guard lock(mutex);batch.swap(commands);}
 static std::uint32_t offset=0,replay_offset=0;
 for(const auto&r:batch){switch(r.command){
 case theater_ui::play:play_replay();break;case theater_ui::pause:pause_replay();break;case theater_ui::stop:emergency_stop();break;case theater_ui::restart:restart_replay();break;
 case theater_ui::toggle_playback:toggle_replay();break;
 case theater_ui::replay_unload:unload_replay();break;
 case theater_ui::seek:seek_replay(r.value);break;case theater_ui::previous:step_replay(-1);break;case theater_ui::next:step_replay(1);break;
 case theater_ui::timescale:{std::lock_guard lock(app.replay_mutex);if(app.replay_player)app.replay_player->set_timescale(theater_timescale::decode(r.value));break;}
 case theater_ui::select:if(!app.game_replay->active())app.selected_replay_actor=r.value;break;
 case theater_ui::page:offset=static_cast<std::uint32_t>(std::min<std::uint64_t>(r.value,UINT32_MAX));break;
 // Same recorder path as F5 and the host Start button; post_command refuses while a replay is active.
 case theater_ui::record_start:post_command(Command::start);break;case theater_ui::record_stop:post_command(Command::stop);break;
 case theater_ui::replay_page:replay_offset=static_cast<std::uint32_t>(std::min<std::uint64_t>(r.value,UINT32_MAX));break;
 // Open by position in the newest-first library; the overlay shows the same list it indexes.
 case theater_ui::replay_open:{fs::path path;{std::lock_guard lock(app.mutex);if(r.value<app.data.replays.size())path=app.data.replays[static_cast<size_t>(r.value)].path;}
  if(path.empty()){log_line("OVERLAY_REPLAY_OPEN refused: index out of range");break;}
  try{open_replay(path);}catch(const std::exception&e){log_line(std::string("OVERLAY_REPLAY_OPEN failed: ")+e.what());
   std::lock_guard lock(app.mutex);app.library_message=std::string("Couldn't load the replay: ")+e.what();app.library_message_error=1;++app.library_message_id;}break;}

 case theater_ui::record_named:{std::string name(r.text,strnlen(r.text,sizeof(r.text)));
  while(!name.empty()&&std::isspace(static_cast<unsigned char>(name.back())))name.pop_back();
  if(name.empty())name=library::default_name();
  {std::lock_guard names(app.names_mutex);app.pending_record_name=name.substr(0,120);}
  log_line("RECORD named start requested: "+name);post_command(Command::start);break;}
 case theater_ui::replay_sort:{app.sort_key=static_cast<std::uint32_t>(r.value/2);app.sort_descending=(r.value%2)!=0;
  library::save_sort(app.root,{static_cast<library::SortKey>(app.sort_key.load()),app.sort_descending.load()});refresh_library();break;}
 case theater_ui::replay_rename:case theater_ui::replay_delete:{
  std::vector<ReplayEntry> list;{std::lock_guard lock(app.mutex);list=app.data.replays;}
  auto message=[&](bool error,std::string text){std::lock_guard lock(app.mutex);app.library_message=std::move(text);app.library_message_error=error;++app.library_message_id;};
  if(r.value>=list.size()){message(true,"That replay is no longer in the list.");break;}
  const auto& entry=list[static_cast<size_t>(r.value)];
  fs::path opened;{std::lock_guard lock(app.replay_mutex);opened=app.opened_replay;}
  if(!opened.empty()&&fs::absolute(entry.path)==opened){message(true,"'"+library::display_name(entry)+"' is loaded. Load another replay first, then try again.");break;}
  const auto result=r.command==theater_ui::replay_rename?library::rename(entry,std::string(r.text,strnlen(r.text,sizeof(r.text))),list):library::recycle(entry);
  message(!result.ok,result.message);refresh_library();break;}
 default:break;}}
 const auto view=playback_view();const auto remote=app.control.state();theater_ui::Snapshot s;
 s.loaded=view.loaded;s.active=view.active;s.phase=static_cast<std::uint32_t>(view.phase);s.time_ns=view.state.timestamp_ns;s.duration_ns=view.summary.duration_ns;s.timescale=view.state.timescale;
 s.selected=app.selected_replay_actor;s.connected=remote.connected;s.player_found=remote.ready;std::copy(remote.live.position.begin(),remote.live.position.end(),s.live_position);
 auto diagnostic=game_launcher::utf8(view.diagnostic);memcpy(s.diagnostic,diagnostic.data(),std::min(diagnostic.size(),sizeof(s.diagnostic)-1));
 s.total=static_cast<std::uint32_t>(app.character_views.size());s.offset=std::min(offset,s.total);s.count=std::min(16u,s.total-s.offset);
 {std::lock_guard lock(app.mutex);const auto rs=app.data.state;s.recording_state=rs==erplay::RecordingState::recording?theater_ui::record_recording:rs==erplay::RecordingState::paused?theater_ui::record_paused:rs==erplay::RecordingState::saving?theater_ui::record_saving:theater_ui::record_idle;s.recording_ns=app.data.active_ns;s.recording_samples=app.data.samples;}
 {std::lock_guard lock(app.mutex);const auto&list=app.data.replays;s.replay_total=static_cast<std::uint32_t>(list.size());s.replay_offset=std::min(replay_offset,s.replay_total);
  s.replay_count=std::min(theater_ui::replay_page_size,s.replay_total-s.replay_offset);
  for(unsigned i=0;i<s.replay_count;++i){const auto&e=list[s.replay_offset+i];auto&o=s.replays[i];const auto name=library::display_name(e);
   const auto file=game_launcher::utf8(e.path.filename().wstring());memcpy(o.file,file.data(),std::min(file.size(),sizeof(o.file)-1));
   memcpy(o.game_version,e.summary.metadata.game_version.data(),std::min(e.summary.metadata.game_version.size(),sizeof(o.game_version)-1));
   o.recorded_unix=e.summary.metadata.recording_start_unix_ns/1'000'000'000ull; // area: recorded from Milestone 3 on
   memcpy(o.name,name.data(),std::min(name.size(),sizeof(o.name)-1));o.duration_ns=e.summary.duration_ns;o.bytes=e.bytes;o.index=s.replay_offset+i;
   o.modified_unix=static_cast<std::uint64_t>(std::max<long long>(0,std::chrono::duration_cast<std::chrono::seconds>(std::chrono::clock_cast<std::chrono::system_clock>(e.modified).time_since_epoch()).count()));
   o.loaded=!app.opened_replay.empty()&&app.opened_replay==fs::absolute(e.path);}}
 for(unsigned i=0;i<s.count;++i){const auto&r=app.character_views[s.offset+i].info.registry;s.actors[i]={r.id,r.native_handle,r.entity_id,r.character_type,r.npc_param,0};}
 s.sort_key=app.sort_key.load();s.sort_descending=app.sort_descending.load();s.name_request=app.name_request.load();
 auto copy=[](char*dst,size_t n,const std::string&src){memcpy(dst,src.data(),std::min(src.size(),n-1));};
 copy(s.default_name,sizeof(s.default_name),library::default_name());
 {std::lock_guard lock(app.mutex);copy(s.library_message,sizeof(s.library_message),app.library_message);s.library_message_id=app.library_message_id;s.library_message_error=app.library_message_error;}
 {std::lock_guard names(app.names_mutex);if(s.recording_state!=theater_ui::record_idle){copy(s.recording_name,sizeof(s.recording_name),app.recording_name);copy(s.recording_path,sizeof(s.recording_path),app.recording_path);}}
 // Bone replay link: the loaded file and the timeline position in source-clock time. Each sample keeps
 // the source time it was recorded at, so pauses during recording map correctly.
 {std::lock_guard lock(app.replay_mutex);if(app.replay_player&&!app.opened_replay.empty()){
  copy(s.loaded_path,sizeof(s.loaded_path),game_launcher::utf8(app.opened_replay.wstring()));
  const auto& st=app.replay_player->state();s.host_playing=st.status==replay::Status::playing;
  if (!s.active) s.phase=s.host_playing?2u:(st.status==replay::Status::paused?3u:0u);
  try{const auto sample=app.replay_player->reader().sample(st.sample_index);s.play_source_ns=sample.source_time_ns+(st.timestamp_ns>sample.replay_time_ns?st.timestamp_ns-sample.replay_time_ns:0);}catch(...){s.play_source_ns=0;}}}
 {std::lock_guard lock(mutex);cached=s;}
}
}
