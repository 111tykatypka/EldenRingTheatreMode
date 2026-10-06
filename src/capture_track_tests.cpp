#include "erplay.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <windows.h>
#include <cassert>
#include <fstream>
#include <iostream>
int main(){using namespace erplay;const auto dir=std::filesystem::temp_directory_path()/("TheaterCaptureTest_"+std::to_string(GetCurrentProcessId())+"_"+std::to_string(GetTickCount64()));std::filesystem::create_directories(dir);auto path=dir/"synthetic_capture.erplay";
 Metadata m;m.format_version=3;m.tags="capture-fidelity-v1";m.title="SYNTHETIC UNIT FIXTURE - NOT GAMEPLAY";
 {Writer w(path,m,2);for(unsigned i=0;i<5;++i){Sample s;s.index=i;s.source_sequence=i+1;s.source_time_ns=100+i*10;s.replay_time_ns=i*10;s.capture=CaptureFrame{};s.capture->values[0]=0x7fc01234;s.capture->valid[0]=1;s.capture->source_drops=i;w.append(s);}auto done=w.finalize();for(auto n:done.capture_record_counts)assert(n==5);assert(done.capture_source_drops==4);}
 Reader reader(path);for(const auto&t:capture_tracks){auto r=reader.capture_at(t.id,21);assert(r&&r->timestamp_ns==20&&r->source_ns==120&&r->sequence==3&&r->source_drops==2);assert(r->values.size()==t.words);}
 auto raw=reader.capture_at(5,0);assert(raw->available(0)&&!raw->available(1)&&raw->values[0]==0x7fc01234);
 // Corruption never silently becomes valid capture data.
 auto bad=dir/"corrupt.erplay";std::filesystem::copy_file(path,bad);{std::fstream out(bad,std::ios::binary|std::ios::in|std::ios::out);out.seekp(250);char byte=0x55;out.write(&byte,1);}bool rejected=false;try{auto s=validate(bad);}catch(...){rejected=true;}assert(rejected);
 // Complete track chunks from an interrupted recording remain recoverable.
 auto interrupted=dir/"interrupted.erplay";std::filesystem::path tmp;{Writer w(interrupted,m,2);tmp=w.temporary_path();for(unsigned i=0;i<2;++i){Sample s;s.index=i;s.source_sequence=i+1;s.source_time_ns=i;s.replay_time_ns=i;s.capture=CaptureFrame{};w.append(s);}}
 auto recovered=recover_incomplete(tmp,dir/"recovered.erplay");for(auto n:recovered.capture_record_counts)assert(n==2);
 // A fidelity session cannot silently finalize without every frame track.
 bool missing_rejected=false;try{Writer w(dir/"missing.erplay",m,2);w.append({});auto done=w.finalize();}catch(...){missing_rejected=true;}assert(missing_rejected);
 // Unsupported extension schemas fail before being queued to storage.
 bool schema_rejected=false;try{Writer w(dir/"bad_schema.erplay",m,2);Sample s;s.capture=CaptureFrame{};s.capture->schema=2;w.append(s);}catch(...){schema_rejected=true;}assert(schema_rejected);
 // Existing ERPLAY02 files have no invented capture track.
 auto old=dir/"legacy.erplay";{Writer w(old,{},2);w.append({});auto s=w.finalize();}assert(!Reader(old).capture_at(5,0));
 std::cout<<"Fidelity raw round trip, unavailable masks, random access, corruption, recovery and legacy PASS. SYNTHETIC fixture: "<<path<<"\n";
}
