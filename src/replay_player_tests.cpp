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
    constexpr double speeds[]{0.1,0.25,0.5,1.0,2.0,4.0};std::uint64_t prior=0;for(double speed:speeds){replay::Player clock{file};clock.set_speed(speed);clock.play(t0);clock.advance(t0+100ms);const auto elapsed=clock.state().timestamp_ns;assert(elapsed>prior);prior=elapsed;}assert(prior==400'000'000ULL);
    if(argc>1){replay::Player real{std::filesystem::path(argv[1])};assert(real.summary().sample_count==4093);assert(real.summary().duration_ns==68'500'000'000ULL);assert(real.summary().chunk_count==7);for(const int pct:{0,50,90,99,100}){const auto target=real.summary().duration_ns*static_cast<std::uint64_t>(pct)/100;real.seek(target);assert(real.state().timestamp_ns==target);assert(real.state().sample_index<real.summary().sample_count);std::cout<<"SCRUB percent="<<pct<<" timestamp_ns="<<real.state().timestamp_ns<<" sample="<<real.state().sample_index<<"\n";}for(int i=0;i<500;++i){real.seek((i%2?real.summary().duration_ns*99/100:real.summary().duration_ns));real.seek(real.summary().duration_ns*90/100);}real.seek(30'000'000'000ULL);const auto first=real.state();assert(first.sample_index>1700&&first.sample_index<1900);real.seek(55'000'000'000ULL);assert(real.state().sample_index>3200);real.seek(0);real.step(1);assert(real.state().timestamp_ns==real.reader().sample(1).replay_time_ns);real.seek(real.summary().duration_ns);real.play(t0);assert(real.state().timestamp_ns==0);real.pause(t0);real.advance(t0+1s);assert(real.state().timestamp_ns==0);std::cout<<"REAL_FIXTURE_PASS samples="<<real.summary().sample_count<<" duration_ns="<<real.summary().duration_ns<<" rate_hz="<<real.summary().actual_rate_hz<<" chunks="<<real.summary().chunk_count<<" rapid_seeks=1000\n";}
    std::cout<<"ReplayPlayer tests passed\n";return 0;
}
int main(int argc,char**argv){try{return run_tests(argc,argv);}catch(const std::exception&e){std::cerr<<"TEST_EXCEPTION: "<<e.what()<<"\n";return 2;}}
