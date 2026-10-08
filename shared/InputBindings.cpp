#include "TheaterHotkeys.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <charconv>
namespace theater_hotkeys {
namespace {
std::mutex files;
std::filesystem::path path(){wchar_t local[32768];auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);if(!n||n>=32768)return {};return std::filesystem::path(local)/L"EldenRingTheaterMode"/L"keybinds.ini";}
bool save(){try{auto p=path();if(p.empty())return false;std::filesystem::create_directories(p.parent_path());auto t=p;t+=L".tmp";{std::ofstream out(t);out<<"THEATER_KEYBINDS_V1\n";for(auto&b:kDefaults)out<<b.id<<'='<<Key(b.action)<<'\n';out.flush();if(!out)return false;}return MoveFileExW(t.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;}catch(...){return false;}}
}
bool Rebind(Action action,std::uint32_t vk,std::string&error){std::lock_guard lock(files);auto index=static_cast<std::size_t>(action);if(index>=overrides.size()||vk>255||!vk){error="Choose a Windows virtual key from 1 to 255";return false;}
 for(auto&b:kDefaults)if(b.action!=action&&!retired(b.action)&&Key(b.action)==vk){error=std::string("Conflict: ")+b.label;return false;}
 auto previous=overrides[index].exchange(vk+1);if(!save()){overrides[index]=previous;error="Keybinding save failed";return false;}error.clear();return true;
}
void Reload(){static std::atomic<ULONGLONG> last=0;auto now=GetTickCount64();if(now-last.load()<1000)return;last=now;std::lock_guard lock(files);
 try{std::ifstream in(path());std::array<std::uint32_t,static_cast<std::size_t>(Action::Count)> keys{};
  if(!DecodeBindings(in,keys))return;
  for(std::size_t i=0;i<keys.size();++i)overrides[i]=keys[i]+1;
 }catch(...){/* Preserve working bindings on invalid/missing storage. */}
}
}
