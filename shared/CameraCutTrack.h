#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>
namespace cinematic {
// Half-open segments; gaps release the camera to the player. No private clock.
enum class CutMode : unsigned { Player=0,Dolly=2 };
struct CameraCut { std::uint64_t id=0,start_ns=0,end_ns=0;CutMode mode=CutMode::Player; };
class CameraCutTrack {
 std::vector<CameraCut> cuts_;
public:
 const auto& cuts()const{return cuts_;}
 bool replace(std::vector<CameraCut> cuts,std::uint64_t duration){
  std::sort(cuts.begin(),cuts.end(),[](auto&a,auto&b){return a.start_ns<b.start_ns;});
  std::vector<std::uint64_t> ids;
  for(std::size_t i=0;i<cuts.size();++i){auto&c=cuts[i];if(!c.id||c.start_ns>=c.end_ns||c.end_ns>duration||(c.mode!=CutMode::Player&&c.mode!=CutMode::Dolly)||(i&&cuts[i-1].end_ns>c.start_ns))return false;ids.push_back(c.id);}
  std::sort(ids.begin(),ids.end());if(std::adjacent_find(ids.begin(),ids.end())!=ids.end())return false;cuts_=std::move(cuts);return true;
 }
 CutMode evaluate(std::uint64_t t)const{
  auto at=std::upper_bound(cuts_.begin(),cuts_.end(),t,[](auto time,const auto&c){return time<c.start_ns;});
  if(at==cuts_.begin())return CutMode::Player;--at;return t<at->end_ns?at->mode:CutMode::Player;
 }
};
}
