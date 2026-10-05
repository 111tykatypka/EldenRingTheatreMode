#include "game_control.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <iostream>
#include <functional>

using namespace game_control;
bool await(const std::function<bool()>& predicate){for(int i=0;i<300;++i){if(predicate())return true;Sleep(10);}return false;}
int main(){
    Packet golden;golden.kind=probe_nudge;golden.sequence=7;golden.timestamp_ns=100;golden.position[0]=0.5f;
    const unsigned char prefix[]{0x54,0x43,0x4d,0x54,1,0,3,0};assert(std::memcmp(&golden,prefix,sizeof(prefix))==0);
    const auto name=L"\\\\.\\pipe\\TheaterMode.ControlTest."+std::to_wstring(GetCurrentProcessId());
    const HANDLE server=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_REJECT_REMOTE_CLIENTS,1,64,64,0,nullptr);assert(server!=INVALID_HANDLE_VALUE);
    std::atomic<unsigned> nudges{},stops{};std::atomic_bool transport_ok{true};
    std::thread mock([&]{if(!ConnectNamedPipe(server,nullptr)&&GetLastError()!=ERROR_PIPE_CONNECTED){transport_ok=false;return;}
        std::uint64_t last=0;std::uint32_t phase=off;
        while(true){Packet request;auto* cursor=reinterpret_cast<char*>(&request);DWORD left=sizeof(request);
            while(left){DWORD got=0;if(!ReadFile(server,cursor,left,&got,nullptr)||got==0){left=0;goto disconnected;}cursor+=got;left-=got;}
            if(request.magic_value!=magic||request.version!=1||request.sequence<=last){transport_ok=false;break;}last=request.sequence;
            if(request.kind==probe_nudge){if(request.position[0]!=0.5f||request.position[1]!=0||request.position[2]!=0||request.quaternion[3]!=1)transport_ok=false;++nudges;phase=observing;}
            if(request.kind==stop){++stops;phase=off;}
            Packet reply;reply.kind=status;reply.sequence=request.sequence;reply.flags=1;reply.state=phase;
            DWORD written=0;if(!WriteFile(server,&reply,sizeof(reply),&written,nullptr)||written!=sizeof(reply))break;
        }
        disconnected:DisconnectNamedPipe(server);
    });
    std::atomic<DWORD> sample_pid{GetCurrentProcessId()};Client client(name);client.start(sample_pid);
    const bool connected=await([&]{return client.state().connected;});
    const bool started=connected&&client.nudge()&&await([&]{return nudges==1&&client.state().phase==observing;});
    const bool rejects_busy=started&&!client.nudge();client.emergency_stop();
    const bool stopped=await([&]{return stops>0&&client.state().phase==off;});
    const bool restarted=stopped&&client.nudge()&&await([&]{return nudges==2;});sample_pid=0;
    const bool disconnected=await([&]{return !client.state().connected;});client.close();
    CancelSynchronousIo(mock.native_handle());mock.join();CloseHandle(server);
    assert(connected&&started&&rejects_busy&&stopped&&restarted&&disconnected&&transport_ok&&stops>=2);
    std::cout<<"Control IPC mock PASS: protocol layout, nudge, busy rejection, emergency STOP, sample disconnect; no game/DLL loaded\n";
}
