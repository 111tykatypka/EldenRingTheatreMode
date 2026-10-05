#include "erplay.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace erplay;
int main() {
    const auto dir=std::filesystem::temp_directory_path()/"erplay-core-tests";
    std::filesystem::remove_all(dir); std::filesystem::create_directories(dir);
    const auto file=dir/u8"тест replay.erplay";
    Metadata m; m.title="Тестовая запись"; m.description="Unicode metadata"; m.tags="test,player"; m.requested_rate_hz=60;
    {
        Writer w(file,m,2);
        for(std::uint64_t i=0;i<5;++i) {
            Sample s; s.index=i; s.replay_time_ns=i*16'700'000; s.source_time_ns=90'000+i*16'700'000;
            s.position={float(i),2.0f,3.0f}; s.orientation={0,0,0,1}; w.append(s);
        }
        assert(std::filesystem::exists(w.temporary_path()));
        const auto done=w.finalize(); assert(done.sample_count==5 && done.chunk_count==3);
        assert(!std::filesystem::exists(w.temporary_path()));
    }
    const auto report=validate(file);
    assert(report.sample_count==5 && report.duration_ns==66'800'000 && report.metadata.title=="Тестовая запись");
    const auto paused_file=dir/"paused.erplay";
    Metadata paused_meta;
    Writer paused_writer(paused_file,paused_meta,4);
    RecordingSession session(paused_writer); session.start();
    Sample live; live.source_time_ns=1'000; session.ingest(live);
    live.source_time_ns=17'000; session.ingest(live);
    session.ingest(live); // equal-clock samples are valid and retained
    session.pause(20'000);
    live.source_time_ns=400'000; session.ingest(live); // ignored while paused
    session.resume(1'020'000);
    live.source_time_ns=1'036'000; session.ingest(live);
    const auto paused_result=session.stop();
    assert(session.state()==RecordingState::ready && paused_result.sample_count==4);
    assert(paused_result.duration_ns==35'000 && paused_result.paused_duration_ns==1'000'000);
    bool rejected=false;
    try { Writer w(dir/"bad.erplay",m); Sample s; s.orientation={0,0,0,0}; w.append(s); }
    catch(const std::invalid_argument&) { rejected=true; }
    assert(rejected);
    const auto interrupted=dir/"interrupted.erplay";
    std::filesystem::path interrupted_tmp;
    {
        Writer w(interrupted,m,2); interrupted_tmp=w.temporary_path();
        for(std::uint64_t i=0;i<5;++i){Sample s;s.index=i;s.source_time_ns=100+i*10;s.replay_time_ns=i*10;s.orientation={0,0,0,1};w.append(s);}
    }
    const auto recovered=dir/"interrupted.recovered.erplay";
    const auto recovered_summary=recover_incomplete(interrupted_tmp,recovered);
    assert(recovered_summary.sample_count==4 && recovered_summary.chunk_count==2);
    assert(std::filesystem::exists(interrupted_tmp) && validate(recovered).sample_count==4);
    const auto same_base=dir/"same-base.erplay";
    std::filesystem::path same_tmp;
    { Writer w(same_base,m,2); same_tmp=w.temporary_path(); for(std::uint64_t i=0;i<4;++i){Sample s;s.index=i;s.source_time_ns=10+i*10;s.replay_time_ns=i*10;s.orientation={0,0,0,1};w.append(s);} }
    const auto original_tmp_bytes=std::filesystem::file_size(same_tmp);
    const auto same_recovered=recover_incomplete(same_tmp,same_base);
    assert(same_recovered.sample_count==4 && std::filesystem::file_size(same_tmp)==original_tmp_bytes && validate(same_base).sample_count==4);
    const auto damaged=dir/"damaged.erplay"; std::filesystem::copy_file(file,damaged);
    { std::fstream f(damaged,std::ios::binary|std::ios::in|std::ios::out); f.seekp(-16,std::ios::end); char c{}; f.write(&c,1); }
    rejected=false; try { (void)validate(damaged); } catch(const std::runtime_error&) { rejected=true; }
    assert(rejected);
    std::filesystem::remove_all(dir);
    std::cout << "ERPLAY tests passed\n";
}
