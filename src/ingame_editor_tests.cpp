#include "ingame_editor_server.hpp"
#include "editor_backend.hpp"
#include "TheaterUiProtocol.h"
#include <iostream>
int main(){using namespace theater;
 auto dir=fs::temp_directory_path()/("TheaterEditorPipe_"+std::to_string(GetCurrentProcessId()));fs::create_directories(dir);app.root=dir;app.logs=dir;
 auto path=dir/L"transport.erplay";{erplay::Writer writer(path,{},2);for(unsigned i=0;i<4;++i){erplay::Sample s;s.index=i;s.source_time_ns=100+i*1'000'000'000ULL;s.replay_time_ns=i*1'000'000'000ULL;writer.append(s);}auto result=writer.finalize();}
 app.replay_player=std::make_unique<replay::Player>(path);app.game_replay=std::make_unique<in_game_replay::Controller>(app.control,[](const auto&){});app.game_pid=GetCurrentProcessId();
 const auto endpoint=std::wstring(theater_ui::pipe)+L"_Test_"+std::to_wstring(GetCurrentProcessId());
 ingame_editor::start(endpoint.c_str());std::atomic_bool done{},ok{true};
 std::thread client([&]{HANDLE h=INVALID_HANDLE_VALUE;for(unsigned i=0;i<100&&h==INVALID_HANDLE_VALUE;++i){h=CreateFileW(endpoint.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)Sleep(10);}if(h==INVALID_HANDLE_VALUE){ok=false;done=true;return;}
  std::uint64_t seq=0;auto send=[&](unsigned command,std::uint64_t value){theater_ui::Request r;r.command=command;r.value=value;r.sequence=++seq;theater_ui::Snapshot s;DWORD n=0;if(!WriteFile(h,&r,sizeof(r),&n,nullptr)||n!=sizeof(r)){ok=false;return;}n=0;unsigned offset=0;while(offset<sizeof(s)){if(!ReadFile(h,reinterpret_cast<char*>(&s)+offset,sizeof(s)-offset,&n,nullptr)||!n){ok=false;return;}offset+=n;}if(s.magic_value!=theater_ui::magic||s.sequence!=seq)ok=false;};
  send(theater_ui::seek,2'000'000'000);send(theater_ui::speed,25);send(theater_ui::poll,0);CloseHandle(h);done=true;
 });
 for(unsigned i=0;i<500&&!done;++i){ingame_editor::poll();Sleep(2);}client.join();ingame_editor::poll();ingame_editor::shutdown();
 if(!ok||app.replay_player->state().timestamp_ns!=2'000'000'000||app.replay_player->state().speed!=.25)return 1;
 std::cout<<"Native editor pipe: accepted matching process, dispatched seek + speed to the existing host Player. No game runtime test.\n";
}
