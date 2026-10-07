#include "ingame_editor_server.hpp"
#include "editor_backend.hpp"
#include "TheaterUiProtocol.h"
#include "replay_library.hpp"
#include <iostream>
int main(){using namespace theater;
 auto dir=fs::temp_directory_path()/("TheaterEditorPipe_"+std::to_string(GetCurrentProcessId()));fs::create_directories(dir);app.root=dir;app.logs=dir;
 auto path=dir/L"transport.erplay";{erplay::Writer writer(path,{},2);for(unsigned i=0;i<4;++i){erplay::Sample s;s.index=i;s.source_time_ns=100+i*1'000'000'000ULL;s.replay_time_ns=i*1'000'000'000ULL;writer.append(s);}auto result=writer.finalize();}
 app.replay_player=std::make_unique<replay::Player>(path);app.game_pid=GetCurrentProcessId();
 const auto endpoint=std::wstring(theater_ui::pipe)+L"_Test_"+std::to_wstring(GetCurrentProcessId());
 ingame_editor::start(endpoint.c_str());std::atomic_bool done{},ok{true};
 std::thread client([&]{HANDLE h=INVALID_HANDLE_VALUE;for(unsigned i=0;i<100&&h==INVALID_HANDLE_VALUE;++i){h=CreateFileW(endpoint.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)Sleep(10);}if(h==INVALID_HANDLE_VALUE){ok=false;done=true;return;}
  std::uint64_t seq=0;auto send=[&](unsigned command,std::uint64_t value){theater_ui::Request r;r.command=command;r.value=value;r.sequence=++seq;theater_ui::Snapshot s;DWORD n=0;if(!WriteFile(h,&r,sizeof(r),&n,nullptr)||n!=sizeof(r)){ok=false;return;}n=0;unsigned offset=0;while(offset<sizeof(s)){if(!ReadFile(h,reinterpret_cast<char*>(&s)+offset,sizeof(s)-offset,&n,nullptr)||!n){ok=false;return;}offset+=n;}if(s.magic_value!=theater_ui::magic||s.sequence!=seq)ok=false;};
  send(theater_ui::seek,2'000'000'000);send(theater_ui::timescale,theater_timescale::encode(.0105));send(theater_ui::poll,0);CloseHandle(h);done=true;
 });
 for(unsigned i=0;i<500&&!done;++i){ingame_editor::poll();Sleep(2);}client.join();ingame_editor::poll();ingame_editor::shutdown();
 if(!ok||app.replay_player->state().timestamp_ns!=2'000'000'000||app.replay_player->state().timescale!=.0105)return 1;
 // Replay Library helpers: safe file names, unique paths, sorting.
 {using namespace theater::library;
  if(safe_stem("  My: run/one?.  ")!=L"My runone"||safe_stem("")!=L"Replay"||safe_stem("CON")!=L"CON_"||safe_stem("<>|")!=L"Replay")return 2;
  if(safe_stem(std::string(200,'a')).size()!=80)return 3;
  const auto lib=dir/L"library";fs::create_directories(lib);
  if(unique_replay_path(lib,L"Run")!=lib/L"Run.erplay")return 4;
  {std::ofstream(lib/L"Run.erplay")<<"x";std::ofstream(lib/L"Run (2).erplay.tmp")<<"x";}
  if(unique_replay_path(lib,L"Run")!=lib/L"Run (3).erplay")return 5;
  if(unique_replay_path(lib,L"Run",lib/L"Run.erplay")!=lib/L"Run.erplay")return 6;
  std::vector<theater::ReplayEntry> list(3);list[0].path=L"b.erplay";list[0].bytes=30;list[0].summary.metadata.title="Bravo";list[0].summary.duration_ns=1;
  list[1].path=L"a.erplay";list[1].bytes=10;list[1].summary.metadata.title="alpha";list[1].summary.duration_ns=3;
  list[2].path=L"Charlie.erplay";list[2].bytes=20;list[2].summary.metadata.title="Elden Ring recording";list[2].summary.duration_ns=2;
  if(display_name(list[2])!="Charlie")return 7;
  sort(list,{SortKey::name,false});if(display_name(list[0])!="alpha"||display_name(list[2])!="Charlie")return 8;
  sort(list,{SortKey::size,true});if(list[0].bytes!=30||list[2].bytes!=10)return 9;
  sort(list,{SortKey::duration,false});if(list[0].summary.duration_ns!=1)return 10;
  save_sort(lib,{SortKey::size,false});const auto order=load_sort(lib);if(order.key!=SortKey::size||order.descending)return 11;
  const auto dup=rename(list[0],"ALPHA",list);if(dup.ok)return 12;const auto empty=rename(list[0],"   ",list);if(empty.ok)return 13;}
 std::cout<<"Native editor pipe: accepted matching process, dispatched seek + speed to the existing host Player. No game runtime test.\n";
}
