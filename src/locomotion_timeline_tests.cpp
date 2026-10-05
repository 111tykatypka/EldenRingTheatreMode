// Semantic fixture only. These labels are NOT inferred from real game IDs.
#include "replay_player.hpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x))throw std::runtime_error(#x);} while(false)
int main(){try{
    namespace fs=std::filesystem;
    auto dir=fs::temp_directory_path()/("theater-locomotion-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(dir);
    const auto file=dir/"normalized.erplay";erplay::Metadata metadata;metadata.format_version=3;
    const erplay::PlayerAction labels[]={erplay::PlayerAction::idle,erplay::PlayerAction::walk,erplay::PlayerAction::run,erplay::PlayerAction::sprint,erplay::PlayerAction::idle};
    {erplay::Writer writer(file,metadata,100);for(unsigned i=0;i<=400;++i){erplay::Sample s;s.index=i;s.source_time_ns=1+std::uint64_t(i)*100'000'000;s.replay_time_ns=s.source_time_ns-1;s.action=erplay::ActionState{};s.action->action=labels[std::min(i/100,4u)];writer.append(s);}CHECK(writer.finalize().action_event_count==5);}
    auto p=std::make_unique<replay::Player>(file);
    CHECK(p->state().current_action.action==labels[0]);
    for(unsigned i=0;i<5;++i){p->seek(std::uint64_t(i)*10'000'000'000);CHECK(p->state().current_action.action==labels[i]);}
    p->seek(9'999'999'999);CHECK(p->state().current_action.action==erplay::PlayerAction::idle);
    p->seek(29'999'999'999);CHECK(p->state().current_action.action==erplay::PlayerAction::run);
    p->seek(10'000'000'000);CHECK(p->state().current_action.action==erplay::PlayerAction::walk);
    auto now=replay::Player::Clock::now();p->restart(now);CHECK(p->state().timestamp_ns==0&&p->state().current_action.action==labels[0]);
    for(unsigned i=1;i<5;++i){p->advance(now+std::chrono::seconds(i*10));CHECK(p->state().current_action.action==labels[i]);}
    p->stop();CHECK(p->state().status==replay::Status::stopped&&p->state().timestamp_ns==0);p.reset();CHECK(!p);
    const auto missing=dir/"v3-missing.erplay";{erplay::Writer w(missing,metadata);erplay::Sample s;w.append(s);(void)w.finalize();}p=std::make_unique<replay::Player>(missing);CHECK(!p->state().has_action&&p->state().current_action.action==erplay::PlayerAction::unknown);
    const auto old=dir/"v2.erplay";{erplay::Writer w(old,{});erplay::Sample s;w.append(s);(void)w.finalize();}p=std::make_unique<replay::Player>(old);CHECK(p->summary().metadata.format_version==2&&!p->state().has_action);p.reset();
    fs::remove(file);fs::remove(missing);fs::remove(old);fs::remove(dir);
    std::cout<<"Normalized Idle/Walk/Run/Sprint timeline, boundaries, backwards seek, restart, stop, unload, missing track, v2 PASS; NOT runtime locomotion\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
