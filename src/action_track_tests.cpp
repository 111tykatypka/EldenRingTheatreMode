#include "replay_player.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <fstream>
#include <cstring>
#include <iterator>
#include <iostream>
#undef assert
#define assert(x) do{if(!(x)){std::cerr<<"FAILED line "<<__LINE__<<": "<<#x<<"\n";throw std::runtime_error("assertion failure");}}while(false)
int run(){
 namespace fs=std::filesystem; const auto dir=fs::temp_directory_path()/"theater-action-tests";fs::remove_all(dir);fs::create_directories(dir);
 erplay::Metadata meta;meta.format_version=3;meta.mod_version="0.3.0";const auto file=dir/"actions.erplay";
 {erplay::Writer w(file,meta,200);for(std::uint64_t i=0;i<1001;++i){erplay::Sample s;s.index=i;s.source_time_ns=1+i*20'000'000;s.replay_time_ns=i*20'000'000;s.position.x=float(i)*.001f;erplay::ActionState a;a.action=i<150?erplay::PlayerAction::walk:i<350?erplay::PlayerAction::run:erplay::PlayerAction::idle;a.flags=erplay::animation_valid;a.animation_id=i<150?10:i<350?20:30;s.action=a;w.append(s);}const auto result=w.finalize();assert(result.sample_count==1001&&result.action_event_count==3);}
 erplay::Reader r(file);assert(r.action_events().size()==3);replay::Player p(file);p.seek(5'000'000'000);assert(p.state().current_action.action==erplay::PlayerAction::run);p.seek(8'000'000'000);assert(p.state().current_action.action==erplay::PlayerAction::idle);p.seek(1'000'000'000);assert(p.state().current_action.action==erplay::PlayerAction::walk);p.restart();assert(p.state().current_action.action==erplay::PlayerAction::walk);auto now=replay::Player::Clock::now();p.play(now);p.advance(now+std::chrono::seconds(6));assert(p.state().current_action.action==erplay::PlayerAction::run);p.advance(now+std::chrono::seconds(9));assert(p.state().current_action.action==erplay::PlayerAction::idle);
 const auto no=dir/"v2.erplay";{erplay::Writer w(no,{});erplay::Sample s;w.append(s);(void)w.finalize();}replay::Player old(no);assert(!old.state().has_action&&old.summary().metadata.format_version==2);
 const auto sync=dir/"sync.erplay";{erplay::Writer w(sync,meta,20);for(unsigned i=0;i<100;++i){erplay::Sample s;s.index=i;s.source_time_ns=1+i*20'000'000;s.replay_time_ns=i*20'000'000;erplay::ActionState a;a.flags=3;a.animation_id=1;a.animation_time=float(i)*.02f;a.animation_length=4;s.action=a;w.append(s);}auto sum=w.finalize();assert(sum.action_event_count==4&&sum.animation_sync_observations);}
 const auto partial=dir/"partial.erplay";fs::path temp;{erplay::Writer w(partial,meta,20);temp=w.temporary_path();for(unsigned i=0;i<45;++i){erplay::Sample s;s.index=i;s.source_time_ns=1+i;s.replay_time_ns=i;s.action=erplay::ActionState{};w.append(s);}}auto rec=erplay::recover_incomplete(temp,dir/"recover.erplay");assert(rec.sample_count==40&&rec.action_event_count==1);
 bool rejected=false;try{erplay::Writer w(dir/"bad.erplay",meta);erplay::Sample s;s.action=erplay::ActionState{};s.action->action=static_cast<erplay::PlayerAction>(99);w.append(s);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
 std::ifstream in(file,std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(in),{}};auto track=bytes.find("TRAK");assert(track!=std::string::npos);
 auto reject=[&](std::string b,const char*name){auto path=dir/name;{std::ofstream o(path,std::ios::binary);o.write(b.data(),b.size());}bool failed=false;try{(void)erplay::validate(path);}catch(const std::exception&){failed=true;}assert(failed);};
 auto recalc=[&](std::string&b){std::uint64_t len{};std::memcpy(&len,b.data()+track+16,8);std::uint32_t c=0xFFFFFFFF;for(std::size_t i=0;i<len;++i){c^=static_cast<unsigned char>(b[track+28+i]);for(int bit=0;bit<8;++bit)c=(c>>1)^(0xEDB88320u&std::uint32_t(0-int(c&1)));}c=~c;std::memcpy(b.data()+track+24,&c,4);};
 auto unknown=bytes;std::uint32_t enum_bad=99;std::memcpy(unknown.data()+track+28+8,&enum_bad,4);recalc(unknown);reject(unknown,"unknown-enum-valid-crc.erplay");
 auto ordering=bytes;std::uint64_t t_first=1,t_second=0;std::memcpy(ordering.data()+track+28,&t_first,8);std::memcpy(ordering.data()+track+28+40,&t_second,8);recalc(ordering);reject(ordering,"regressing-events-valid-crc.erplay");
 {std::ifstream f(temp,std::ios::binary);std::string torn{std::istreambuf_iterator<char>(f),{}};auto at=torn.find("TRAK");assert(at!=std::string::npos);const auto source=dir/"torn-action.erplay.tmp";{std::ofstream o(source,std::ios::binary);o.write(torn.data(),at+30);}auto recovered=erplay::recover_incomplete(source,dir/"torn-recovered.erplay");assert(recovered.sample_count==20&&recovered.action_event_count==0);}
 auto damaged=bytes;damaged[track+28+8]^=1;reject(damaged,"crc.erplay");reject(bytes.substr(0,track+30),"truncated.erplay");
 // CRC covers payload, allowing a future optional type to be skipped.
 auto optional=bytes;optional[track+4]=99;auto optional_file=dir/"optional.erplay"; // Footer count must reflect the skipped known events.
 std::uint64_t count=1;std::memcpy(optional.data()+optional.size()-8,&count,8);{std::ofstream o(optional_file,std::ios::binary);o.write(optional.data(),optional.size());}assert(erplay::validate(optional_file).action_event_count==1);
 optional[track+8]=1;reject(optional,"required.erplay");
 std::cout<<"Action track / v2 compatibility / seek / recovery / integrity passed\n";return 0;
}

int main(){try{return run();}catch(const std::exception&e){std::cerr<<"Action test exception: "<<e.what()<<std::endl;return 1;}}
