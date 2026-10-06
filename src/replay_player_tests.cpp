#include "erplay.hpp"
#include "replay_player.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>

using namespace std::chrono_literals;
int run_tests(int argc,char**argv){
    const auto dir=std::filesystem::temp_directory_path()/"theater-replay-player-tests";std::filesystem::remove_all(dir);std::filesystem::create_directories(dir);
    auto file=dir/"clock.erplay";erplay::Metadata meta;meta.title="Replay clock test";
    {erplay::Writer w(file,meta,2);for(std::uint64_t i=0;i<5;++i){erplay::Sample s;s.index=i;s.source_time_ns=100+i*100'000'000;s.replay_time_ns=i*100'000'000;s.position={float(i),float(2*i),0};s.orientation={0,0,0,1};w.append(s);}const auto saved=w.finalize();assert(saved.sample_count==5);}
    erplay::Reader r(file);assert(r.summary().sample_count==5);assert(r.sample(3).position.x==3);assert(r.lower_sample(250'000'000)==2);auto [a,b]=r.bracket(250'000'000);assert(a.position.x==2&&b.position.x==3);
    replay::Player p(file);const auto t0=replay::Player::Clock::time_point{};p.play(t0);p.advance(t0+250ms);assert(p.state().timestamp_ns==250'000'000);assert(std::abs(p.state().position.x-2.5f)<0.001f);p.pause(t0+300ms);const auto paused=p.state().timestamp_ns;assert(p.state().status==replay::Status::paused);p.advance(t0+4s);assert(p.state().timestamp_ns==paused);
    p.set_speed(0.5,t0+4s);p.play(t0+4s);p.advance(t0+4s+200ms);assert(p.state().timestamp_ns==paused+100'000'000);p.seek(0);assert(p.state().timestamp_ns==0);p.step(1);assert(p.state().timestamp_ns==100'000'000);p.seek(400'000'000);assert(p.state().sample_index==4);
    p.stop();assert(p.state().status==replay::Status::stopped&&p.state().timestamp_ns==0);p.restart(t0+5s);assert(p.state().status==replay::Status::playing);p.advance(t0+5s+50ms);assert(p.state().timestamp_ns==25'000'000);
    auto q=replay::slerp({0,0,0,1},{0,1,0,0},0.5);const double norm=std::sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w);assert(std::abs(norm-1.0)<1e-5);
    auto side=dir/"clock.bookmarks";replay::BookmarkStore bm(side);bm.add(300);bm.add(100);bm.add(300);bm.save();replay::BookmarkStore loaded(side);loaded.load();assert(loaded.timestamps().size()==2&&loaded.timestamps()[0]==100&&loaded.timestamps()[1]==300);loaded.erase(0);loaded.save();replay::BookmarkStore again(side);again.load();assert(again.timestamps().size()==1&&again.timestamps()[0]==300);
    // Explicit capture producer support; unknown producer versions still fail closed.
    for(const auto version : {"0.7.0-fidelity1", "unknown-future"}) {
        auto capture_file=dir/(std::string(version)+".erplay");erplay::Metadata capture_meta;capture_meta.format_version=3;capture_meta.mod_version=version;
        {erplay::Writer w(capture_file,capture_meta,2);w.append({});w.finalize();}
        bool accepted=false;try{replay::Player capture(capture_file);accepted=true;}catch(const std::runtime_error&){}
        assert(accepted==(std::string(version)=="0.7.0-fidelity1"));
    }
    if(argc==3&&std::string(argv[1])=="--capture-real") {
        replay::Player real{std::filesystem::path(argv[2])};const auto& summary=real.summary();
        assert(summary.metadata.mod_version=="0.7.0-fidelity1");assert(summary.sample_count>0);
        for(auto count:summary.capture_record_counts)assert(count==summary.sample_count);
        for(auto time:{std::uint64_t(0),summary.duration_ns/2,summary.duration_ns}) {
            real.seek(time);assert(real.state().timestamp_ns==time);
            for(unsigned track=5;track<=13;++track)assert(real.reader().capture_at(track,time));
        }
        std::cout<<"REAL_CAPTURE_VALIDATED samples="<<summary.sample_count<<" duration_ns="<<summary.duration_ns<<" actual_hz="<<summary.actual_rate_hz<<" chunks="<<summary.chunk_count<<" tracks=9\n";
    } else
    if(argc>1){replay::Player real{std::filesystem::path(argv[1])};assert(real.summary().sample_count==4093);assert(real.summary().duration_ns==68'500'000'000ULL);assert(real.summary().chunk_count==7);real.seek(30'000'000'000ULL);const auto first=real.state();assert(first.sample_index>1700&&first.sample_index<1900);real.seek(55'000'000'000ULL);assert(real.state().sample_index>3200);real.seek(0);real.step(1);assert(real.state().timestamp_ns==real.reader().sample(1).replay_time_ns);std::cout<<"REAL_FIXTURE_PASS samples="<<real.summary().sample_count<<" duration_ns="<<real.summary().duration_ns<<" rate_hz="<<real.summary().actual_rate_hz<<" chunks="<<real.summary().chunk_count<<"\n";}
    std::cout<<"ReplayPlayer tests passed\n";return 0;
}
int main(int argc,char**argv){try{return run_tests(argc,argv);}catch(const std::exception&e){std::cerr<<"TEST_EXCEPTION: "<<e.what()<<"\n";return 2;}}
