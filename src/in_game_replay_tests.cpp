#include "in_game_replay.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using namespace std::chrono_literals;
using Clock=replay::Player::Clock;
bool wait_for(const std::function<bool()>& predicate){for(int i=0;i<300;++i){if(predicate())return true;Sleep(10);}return false;}

// Transport/coordinator mock only: no production pipe, DLL, hook or game is used.
class MockGame {
public:
    explicit MockGame(game_control::Transform live):live_(live){
        name=L"\\\\.\\pipe\\TheaterMode.ReplayTest."+std::to_wstring(GetCurrentProcessId());
        pipe_=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_REJECT_REMOTE_CLIENTS,1,96,96,0,nullptr);
        assert(pipe_!=INVALID_HANDLE_VALUE);thread_=std::thread([this]{run();});
    }
    ~MockGame(){CancelSynchronousIo(thread_.native_handle());thread_.join();CloseHandle(pipe_);}
    void live(game_control::Transform transform){std::lock_guard lock(mutex_);live_=transform;}
    unsigned count(std::uint16_t kind){std::lock_guard lock(mutex_);unsigned count=0;for(const auto&p:commands_)if(p.kind==kind)++count;return count;}
    game_control::Packet last(std::uint16_t kind){std::lock_guard lock(mutex_);for(auto it=commands_.rbegin();it!=commands_.rend();++it)if(it->kind==kind)return *it;return {};}
    std::wstring name;
private:
    HANDLE pipe_;std::thread thread_;std::mutex mutex_;game_control::Transform live_;
    std::vector<game_control::Packet> commands_;std::uint64_t session_{},replay_ns_{},applied_{};std::uint32_t phase_{};
    void run(){
        if(!ConnectNamedPipe(pipe_,nullptr)&&GetLastError()!=ERROR_PIPE_CONNECTED)return;
        std::uint64_t sequence=0;
        while(true){game_control::Packet request;auto*cursor=reinterpret_cast<char*>(&request);DWORD left=sizeof(request);
            while(left){DWORD got=0;if(!ReadFile(pipe_,cursor,left,&got,nullptr)||!got)goto done;cursor+=got;left-=got;}
            assert(request.version==2&&request.sequence>sequence);sequence=request.sequence;
            game_control::Packet reply;
            {std::lock_guard lock(mutex_);commands_.push_back(request);
                if(request.kind==game_control::stop){phase_=game_control::inactive;session_=0;}
                if(request.kind==game_control::replay_begin||request.kind==game_control::replay_apply||request.kind==game_control::replay_finish){
                    session_=request.session;replay_ns_=request.replay_timestamp_ns;applied_=request.sequence;
                    std::copy(std::begin(request.position),std::end(request.position),live_.position.begin());std::copy(std::begin(request.quaternion),std::end(request.quaternion),live_.quaternion.begin());
                    phase_=request.kind==game_control::replay_finish?game_control::finished:request.replay_state;
                }
                reply.kind=game_control::status;reply.sequence=sequence;reply.timestamp_ns=GetTickCount64()*1'000'000ULL;reply.flags=3;
                reply.session=session_;reply.replay_timestamp_ns=replay_ns_;reply.replay_state=phase_;reply.applied_sequence=applied_;
                std::copy(live_.position.begin(),live_.position.end(),reply.position);std::copy(live_.quaternion.begin(),live_.quaternion.end(),reply.quaternion);
            }
            DWORD written=0;if(!WriteFile(pipe_,&reply,sizeof(reply),&written,nullptr)||written!=sizeof(reply))break;
        }
        done:DisconnectNamedPipe(pipe_);
    }
};
int run_tests(int argc,wchar_t**argv){
    const auto dir=std::filesystem::temp_directory_path()/(L"TheaterMode-Игровой-replay-tests-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(dir);const auto file=dir/L"coordinator.erplay";
    {erplay::Metadata m;m.title="Mock coordinator test";erplay::Writer writer(file,m,100);
        for(std::uint64_t i=0;i<=500;++i){erplay::Sample s;s.index=i;s.replay_time_ns=i*20'000'000;s.source_time_ns=100+i*20'000'000;s.position={float(i)*0.004f,0,0};const double angle=double(i)*0.002;s.orientation={0,float(std::sin(angle)),0,float(std::cos(angle))};writer.append(s);}(void)writer.finalize();}
    auto player=std::make_unique<replay::Player>(file);
    MockGame game({});std::atomic<DWORD> sample_pid{GetCurrentProcessId()};game_control::Client client(game.name);client.start(sample_pid);
    assert(wait_for([&]{return client.state().connected&&client.state().ready;}));
    in_game_replay::Controller controller(client,[&](const auto&line){if(argc>1)std::cerr<<line<<std::endl;});auto now=Clock::now();
    assert(controller.play(*player,5'000'000'000,now));
    assert(wait_for([&]{controller.tick(now);return controller.phase()==in_game_replay::Phase::playing;}));
    assert(player->state().timestamp_ns==0); // BEGIN acknowledgement precedes clock start.
    controller.tick(now+1s);assert(player->state().timestamp_ns==1'000'000'000);
    assert(wait_for([&]{return game.last(game_control::replay_apply).replay_timestamp_ns==1'000'000'000;}));
    controller.pause(now+2s);assert(player->state().timestamp_ns==2'000'000'000);
    assert(wait_for([&]{return client.state().replay_phase==game_control::paused;}));
    controller.tick(now+10s);assert(player->state().timestamp_ns==2'000'000'000);
    assert(controller.play(*player,5'000'000'000,now+10s));controller.tick(now+11s);
    assert(player->state().timestamp_ns==3'000'000'000); // One host clock, pause wall time excluded.
    assert(wait_for([&]{return game.last(game_control::replay_apply).replay_timestamp_ns==3'000'000'000;}));
    const auto q=game.last(game_control::replay_apply);assert(std::abs(q.position[0]-0.6f)<0.001f&&std::abs(q.quaternion[1]-std::sin(0.3))<0.001f);
    controller.restart(*player,5'000'000'000,now+12s);
    assert(wait_for([&]{controller.tick(now+12s);return controller.phase()==in_game_replay::Phase::playing;}));
    assert(player->state().timestamp_ns==0);
    controller.tick(now+18s);
    assert(wait_for([&]{controller.tick(now+18s);return controller.phase()==in_game_replay::Phase::finished;}));
    assert(player->state().timestamp_ns==5'000'000'000);assert(game.last(game_control::replay_finish).replay_timestamp_ns==5'000'000'000);
    const auto apply_count=game.count(game_control::replay_apply);controller.tick(now+19s);assert(game.count(game_control::replay_apply)==apply_count);
    controller.stop();assert(wait_for([&]{return !client.state().pending&&client.state().replay_phase==game_control::inactive;}));
    assert(!client.apply_replay(999,0,{},false));
    game_control::Transform bad;bad.position[0]=std::numeric_limits<float>::quiet_NaN();assert(!client.begin_replay(999,bad));
    game.live({{500,0,0},{0,0,0,1}});assert(wait_for([&]{return client.state().live.position[0]==500;}));
    assert(!controller.play(*player,5'000'000'000,now));assert(controller.phase()==in_game_replay::Phase::error);
    assert(wait_for([&]{return !client.state().pending;}));
    if(argc>1){
        auto real=std::make_unique<replay::Player>(std::filesystem::path(argv[1]));const auto first=real->reader().sample(0);
        game.live({{first.position.x,first.position.y,first.position.z},{first.orientation.x,first.orientation.y,first.orientation.z,first.orientation.w}});
        assert(wait_for([&]{return std::abs(client.state().live.position[0]-first.position.x)<0.001f;}));
        now=Clock::now();assert(controller.play(*real,5'000'000'000,now));
        assert(wait_for([&]{controller.tick(now);return controller.phase()==in_game_replay::Phase::playing;}));
        controller.tick(now+1s);controller.pause(now+2s);
        assert(wait_for([&]{return client.state().replay_phase==game_control::paused;}));
        assert(controller.play(*real,5'000'000'000,now+3s));controller.tick(now+7s);
        assert(wait_for([&]{controller.tick(now+7s);return controller.phase()==in_game_replay::Phase::finished;}));
        assert(real->state().timestamp_ns==5'000'000'000);
        const auto sent=game.last(game_control::replay_finish);const auto state=real->state();
        assert(sent.position[0]==state.position.x&&sent.position[1]==state.position.y&&sent.position[2]==state.position.z);
        assert(sent.quaternion[0]==state.orientation.x&&sent.quaternion[1]==state.orientation.y&&sent.quaternion[2]==state.orientation.z&&sent.quaternion[3]==state.orientation.w);
        std::cout<<"REAL_FIXTURE_MOCK_PASS samples="<<real->summary().sample_count<<" duration_ns="<<real->summary().duration_ns<<" chunks="<<real->summary().chunk_count<<" first=("<<first.position.x<<','<<first.position.y<<','<<first.position.z<<") first_5_seconds_only; not an in-game test\n";
        controller.stop();assert(wait_for([&]{return !client.state().pending;}));
    }
    game.live({});assert(wait_for([&]{return std::abs(client.state().live.position[0])<0.001f;}));
    now=Clock::now();assert(controller.play(*player,5'000'000'000,now));
    assert(wait_for([&]{controller.tick(now);return controller.phase()==in_game_replay::Phase::playing;}));
    controller.stop();assert(wait_for([&]{return client.state().replay_phase==game_control::inactive&&!client.state().pending;}));
    const auto stopped_count=game.count(game_control::replay_apply);controller.tick(now+1s);assert(game.count(game_control::replay_apply)==stopped_count);
    assert(controller.play(*player,5'000'000'000,now));
    assert(wait_for([&]{controller.tick(now);return controller.phase()==in_game_replay::Phase::playing;}));
    sample_pid=0;assert(wait_for([&]{return !client.state().connected;}));controller.tick(now+1s);assert(controller.phase()==in_game_replay::Phase::error);
    client.close();
    std::cout<<"In-game coordinator MOCK PASS: begin ACK, interpolation/SLERP, pause/resume, restart STOP ACK, exact 5-second finish, STOP, displacement guard, malformed local target and disconnect. No game or DLL loaded.\n";
    return 0;
}

int wmain(int argc,wchar_t**argv){
    try{
        if(argc>1&&!std::filesystem::is_regular_file(argv[1])){std::wcerr<<L"REAL_FIXTURE_MISSING: "<<argv[1]<<L"\n";return 2;}
        return run_tests(argc,argv);
    }catch(const std::exception&e){std::cerr<<"TEST_EXCEPTION: "<<e.what()<<"\n";return 2;}
}
