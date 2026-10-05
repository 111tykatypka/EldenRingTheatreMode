// Independent DX12 backend. Design references: FreecamMod and dx12-imgui-overlay.
// No game offsets or character writes are implemented in this translation unit.
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include "TheaterUiProtocol.h"
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <fstream>
#include <filesystem>
using Microsoft::WRL::ComPtr;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
using Present=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using Resize=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
using Create=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using CreateHwnd=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**);
struct Frame{ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12Resource> buffer;D3D12_CPU_DESCRIPTOR_HANDLE rtv{};UINT64 fence{};};
struct Input{HWND hwnd;UINT msg;WPARAM w;LPARAM l;};
class TheaterRenderBackend {
public:
 std::recursive_mutex graphics;std::mutex ipc,input_mutex;
 BOOL (WINAPI* set_cursor_pos)(int,int){};Present present{};Resize resize{};Create create{};CreateHwnd create_hwnd{};
 std::vector<void*> targets;ComPtr<IDXGISwapChain3> chain;ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12Device> device;
 ComPtr<ID3D12DescriptorHeap> rtvs,srvs;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
 std::vector<Frame> frames;UINT64 fence_value{};HANDLE fence_event{};HWND hwnd{};WNDPROC previous_proc{};
 ImGuiContext* context{};bool win32_ready{},dx12_ready{},failed{};std::vector<bool> descriptors;
 std::deque<Input> inputs;std::deque<theater_ui::Request> commands;theater_ui::Snapshot snapshot;
 std::atomic_int mode{1};std::atomic_bool running{true},ui_ready{},capture_mouse{},capture_keyboard{};std::thread client;
 void (*emergency)(){};
 void log(const char*message){wchar_t path[MAX_PATH]{};GetTempPathW(MAX_PATH,path);std::ofstream file(std::filesystem::path(path)/L"TheaterModeRender.log",std::ios::app);file<<GetTickCount64()<<" "<<message<<'\n';}
 void command(std::uint32_t kind,std::uint64_t value=0){if(kind==theater_ui::stop&&emergency)emergency();std::lock_guard lock(ipc);if(commands.size()<32){theater_ui::Request r;r.command=kind;r.value=value;commands.push_back(r);}}
 bool transfer(HANDLE h,void*p,DWORD n,bool write){auto*c=static_cast<char*>(p);while(n){DWORD got=0;if(!(write?WriteFile(h,c,n,&got,nullptr):ReadFile(h,c,n,&got,nullptr))||!got)return false;c+=got;n-=got;}return true;}
 void ipc_worker(){HANDLE h=INVALID_HANDLE_VALUE;std::uint64_t sequence=0;while(running){
  if(h==INVALID_HANDLE_VALUE){h=CreateFileW(theater_ui::pipe,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE){Sleep(100);continue;}}
  theater_ui::Request r;{std::lock_guard lock(ipc);if(!commands.empty()){r=commands.front();commands.pop_front();}}r.sequence=++sequence;theater_ui::Snapshot s;
  if(!transfer(h,&r,sizeof(r),true)||!transfer(h,&s,sizeof(s),false)||s.magic_value!=theater_ui::magic||s.version!=1||s.sequence!=r.sequence||s.count>16||!std::isfinite(s.playback_speed)){
   CloseHandle(h);h=INVALID_HANDLE_VALUE;{std::lock_guard lock(ipc);snapshot={};commands.clear();}Sleep(100);continue;}
  {std::lock_guard lock(ipc);snapshot=s;}Sleep(50);
 }if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
 void adopt(IDXGISwapChain*sc,IUnknown*unknown){ComPtr<ID3D12CommandQueue> q;ComPtr<IDXGISwapChain3> c;
  if(FAILED(unknown->QueryInterface(IID_PPV_ARGS(&q)))||q->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT||FAILED(sc->QueryInterface(IID_PPV_ARGS(&c))))return;
  std::lock_guard lock(graphics);if(chain)return;chain=c;queue=q;log("DX12_QUEUE_BOUND: queue supplied to native CreateSwapChain; no guessed queue");}
 bool wait_gpu(){if(!fence||!queue)return true;if(FAILED(queue->Signal(fence.Get(),++fence_value)))return false;
  if(fence->GetCompletedValue()>=fence_value)return true;
  return SUCCEEDED(fence->SetEventOnCompletion(fence_value,fence_event))&&WaitForSingleObject(fence_event,2000)==WAIT_OBJECT_0;}
 void release_resources(){ui_ready=false;if(context){ImGui::SetCurrentContext(context);if(dx12_ready)ImGui_ImplDX12_Shutdown();if(win32_ready)ImGui_ImplWin32_Shutdown();ImGui::DestroyContext(context);}
  context=nullptr;dx12_ready=win32_ready=false;frames.clear();list.Reset();rtvs.Reset();srvs.Reset();fence.Reset();device.Reset();descriptors.clear();if(fence_event){CloseHandle(fence_event);fence_event=nullptr;}}
 bool initialize_resources(){DXGI_SWAP_CHAIN_DESC desc{};if(!chain||!queue||FAILED(chain->GetDesc(&desc))||FAILED(chain->GetDevice(IID_PPV_ARGS(&device)))||!desc.BufferCount)return false;
  hwnd=desc.OutputWindow;if(!IsWindow(hwnd))return false;
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=desc.BufferCount;
  if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtvs))))return false;
  hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=128;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  if(FAILED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srvs))))return false;descriptors.assign(128,false);
  frames.resize(desc.BufferCount);auto cpu=rtvs->GetCPUDescriptorHandleForHeapStart();auto stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for(UINT i=0;i<desc.BufferCount;++i){auto&f=frames[i];if(FAILED(chain->GetBuffer(i,IID_PPV_ARGS(&f.buffer)))||FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&f.allocator))))return false;f.rtv={cpu.ptr+i*stride};device->CreateRenderTargetView(f.buffer.Get(),nullptr,f.rtv);}
  if(FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,frames[0].allocator.Get(),nullptr,IID_PPV_ARGS(&list)))||FAILED(list->Close())||FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))return false;
  fence_value=0;fence_event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!fence_event)return false;
  context=ImGui::CreateContext();ImGui::SetCurrentContext(context);auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;ImGui::StyleColorsDark();auto dpi=GetDpiForWindow(hwnd)/96.f;ImGui::GetStyle().FontScaleDpi=dpi;ImGui::GetStyle().ScaleAllSizes(dpi);
  win32_ready=ImGui_ImplWin32_Init(hwnd);if(!win32_ready)return false;
  ImGui_ImplDX12_InitInfo info;info.Device=device.Get();info.CommandQueue=queue.Get();info.NumFramesInFlight=static_cast<int>(desc.BufferCount);info.RTVFormat=desc.BufferDesc.Format;info.SrvDescriptorHeap=srvs.Get();info.UserData=this;
  info.SrvDescriptorAllocFn=[](ImGui_ImplDX12_InitInfo*i,D3D12_CPU_DESCRIPTOR_HANDLE*c,D3D12_GPU_DESCRIPTOR_HANDLE*g){auto*b=static_cast<TheaterRenderBackend*>(i->UserData);auto stride=b->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);for(UINT n=0;n<b->descriptors.size();++n)if(!b->descriptors[n]){b->descriptors[n]=true;c->ptr=b->srvs->GetCPUDescriptorHandleForHeapStart().ptr+n*stride;g->ptr=b->srvs->GetGPUDescriptorHandleForHeapStart().ptr+n*stride;return;}*c={};*g={};b->failed=true;};
  info.SrvDescriptorFreeFn=[](ImGui_ImplDX12_InitInfo*i,D3D12_CPU_DESCRIPTOR_HANDLE c,D3D12_GPU_DESCRIPTOR_HANDLE){auto*b=static_cast<TheaterRenderBackend*>(i->UserData);auto stride=b->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);auto index=(c.ptr-b->srvs->GetCPUDescriptorHandleForHeapStart().ptr)/stride;if(index<b->descriptors.size())b->descriptors[index]=false;};
  dx12_ready=ImGui_ImplDX12_Init(&info);if(!dx12_ready)return false;
  if(!previous_proc){SetLastError(0);previous_proc=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(wndproc)));if(!previous_proc)return false;}
  ui_ready=true;log("DX12_IMGUI_INITIALIZED; Insert cycles Overlay/Editor/Clean; runtime visuals require user verification");return true;
 }
 void draw(){theater_ui::Snapshot s;{std::lock_guard lock(ipc);s=snapshot;}auto&io=ImGui::GetIO();io.MouseDrawCursor=mode.load()==2;
  const bool editor=mode.load()==2;ImGui::SetNextWindowPos(ImVec2(10,10),ImGuiCond_FirstUseEver);ImGui::SetNextWindowSize(ImVec2(editor?560.f:460.f,editor?380.f:180.f),ImGuiCond_FirstUseEver);
  ImGui::Begin(editor?"Theater Editor — experimental":"Theater Transport — experimental");
  ImGui::Text("Host clock %.3f / %.3f seconds",s.time_ns/1e9,s.duration_ns/1e9);ImGui::Text("Game %s | Player %s",s.connected?"CONNECTED":"WAITING",s.player_found?"FOUND":"WAITING");
  ImGui::BeginDisabled(!s.loaded);if(ImGui::Button("Play / Resume"))command(theater_ui::play);ImGui::SameLine();if(ImGui::Button("Pause"))command(theater_ui::pause);ImGui::SameLine();if(ImGui::Button("Stop / F6"))command(theater_ui::stop);
  if(ImGui::Button("Restart"))command(theater_ui::restart);ImGui::SameLine();if(ImGui::Button("Previous tick"))command(theater_ui::previous);ImGui::SameLine();if(ImGui::Button("Next tick"))command(theater_ui::next);
  constexpr double speeds[]{.1,.25,.5,1,2,4};const char*labels[]{"0.1x","0.25x","0.5x","1x","2x","4x"};int selected=3;for(int i=0;i<6;++i)if(s.playback_speed==speeds[i])selected=i;
  if(ImGui::Combo("Playback speed",&selected,labels,6))command(theater_ui::speed,static_cast<std::uint64_t>(speeds[selected]*100));
  double time=s.time_ns/1e9,lo=0,hi=s.duration_ns/1e9;if(ImGui::SliderScalar("Replay time",ImGuiDataType_Double,&time,&lo,&hi,"%.3f s"))command(theater_ui::seek,static_cast<std::uint64_t>(time*1e9));ImGui::EndDisabled();
  ImGui::TextWrapped("%s",s.diagnostic);if(editor){ImGui::Separator();ImGui::TextUnformatted("Scene — recorded actor identities (runtime resolution UNKNOWN)");
   ImGuiListClipper clip;clip.Begin(static_cast<int>(s.count));while(clip.Step())for(int i=clip.DisplayStart;i<clip.DisplayEnd;++i){auto&a=s.actors[i];char label[128];snprintf(label,sizeof(label),"Actor %llu / entity %u / NPC %d",a.id,a.entity,a.npc);if(ImGui::Selectable(label,a.id==s.selected))command(theater_ui::select,a.id);}
   if(ImGui::Button("Previous actors"))command(theater_ui::page,s.offset>=16?s.offset-16:0);ImGui::SameLine();if(ImGui::Button("Next actors"))command(theater_ui::page,std::min(s.offset+16,s.total));
   ImGui::Text("Live XYZ %.3f %.3f %.3f",s.live_position[0],s.live_position[1],s.live_position[2]);ImGui::TextUnformatted("Ownership / AI / grounding: inspect runtime trace; UNKNOWN in UI.");
   ImGui::TextUnformatted("Free Camera / Dolly / world loading: NOT IMPLEMENTED.");ImGui::TextWrapped("Seek and stepping stop native replay writes. Editor time changes; world reconstruction is not available.");
  }ImGui::End();capture_mouse=io.WantCaptureMouse;capture_keyboard=io.WantCaptureKeyboard;
 }
 void render(IDXGISwapChain*sc,UINT flags){if(flags&DXGI_PRESENT_TEST)return;std::lock_guard lock(graphics);if(failed||!chain||sc!=static_cast<IDXGISwapChain*>(chain.Get()))return;
  if(!context&&!initialize_resources()){log("DX12_RESOURCE_INIT_FAILED; overlay disabled; replay integration unchanged");release_resources();failed=true;return;}
  ImGui::SetCurrentContext(context);{std::lock_guard lock(input_mutex);while(!inputs.empty()){auto m=inputs.front();inputs.pop_front();ImGui_ImplWin32_WndProcHandler(m.hwnd,m.msg,m.w,m.l);}}
  const auto index=chain->GetCurrentBackBufferIndex();if(index>=frames.size())return;auto&f=frames[index];if(f.fence&&fence->GetCompletedValue()<f.fence)return; // Do not stall game Present.
  ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();if(mode.load()!=0)draw();else{capture_mouse=false;capture_keyboard=false;}ImGui::Render();if(mode.load()==0)return;
  if(FAILED(f.allocator->Reset())||FAILED(list->Reset(f.allocator.Get(),nullptr))){failed=true;return;}
  D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={f.buffer.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET};list->ResourceBarrier(1,&barrier);
  list->OMSetRenderTargets(1,&f.rtv,FALSE,nullptr);ID3D12DescriptorHeap*heap=srvs.Get();list->SetDescriptorHeaps(1,&heap);ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),list.Get());std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);list->ResourceBarrier(1,&barrier);
  if(FAILED(list->Close())){failed=true;return;}ID3D12CommandList*cmd=list.Get();queue->ExecuteCommandLists(1,&cmd);f.fence=++fence_value;if(FAILED(queue->Signal(fence.Get(),f.fence))){failed=true;log("DX12_QUEUE_SIGNAL_FAILED; rendering disabled");}
 }
 static LRESULT CALLBACK wndproc(HWND,UINT,WPARAM,LPARAM);
};
// Loaded for process lifetime, like the existing recurring Rust task callbacks.
// Dynamic FreeLibrary is not supported; explicit shutdown is outside DllMain.
TheaterRenderBackend&backend(){static auto*b=new TheaterRenderBackend;return *b;}
LRESULT CALLBACK TheaterRenderBackend::wndproc(HWND h,UINT m,WPARAM w,LPARAM l){auto&b=backend();
 if(m==WM_KEYDOWN&&!(l&(1LL<<30))&&w==VK_INSERT){b.mode=(b.mode.load()+1)%3;return 0;}
 if(m==WM_KEYDOWN&&w==VK_F6){b.command(theater_ui::stop);return 0;}
 if(b.mode.load()){std::lock_guard lock(b.input_mutex);if(b.inputs.size()<256)b.inputs.push_back({h,m,w,l});}
 if(b.mode.load()&&((b.capture_mouse&&(m>=WM_MOUSEFIRST&&m<=WM_MOUSELAST))||(b.capture_keyboard&&(m==WM_KEYDOWN||m==WM_KEYUP||m==WM_CHAR))))return 0;
 return CallWindowProcW(b.previous_proc,h,m,w,l);
}
BOOL WINAPI on_set_cursor_pos(int x,int y){auto&b=backend();if(b.mode.load()==2&&b.ui_ready.load())return TRUE;return b.set_cursor_pos(x,y);}
HRESULT STDMETHODCALLTYPE on_present(IDXGISwapChain*s,UINT interval,UINT flags){try{backend().render(s,flags);}catch(...){std::lock_guard lock(backend().graphics);backend().failed=true;backend().log("DX12_RENDER_EXCEPTION; overlay disabled; forwarding original Present");}return backend().present(s,interval,flags);}
HRESULT STDMETHODCALLTYPE on_resize(IDXGISwapChain*s,UINT n,UINT w,UINT h,DXGI_FORMAT f,UINT flags){auto&b=backend();std::lock_guard lock(b.graphics);
 if(b.chain&&s==static_cast<IDXGISwapChain*>(b.chain.Get())&&b.context){if(!b.wait_gpu()){b.failed=true;b.log("DX12_RESIZE_GPU_TIMEOUT; resources retained; resize refused safely");return DXGI_ERROR_WAS_STILL_DRAWING;}b.release_resources();b.log("DX12_RESIZE: resources released; recreate on next Present");}
 return b.resize(s,n,w,h,f,flags);
}
HRESULT STDMETHODCALLTYPE on_create(IDXGIFactory*f,IUnknown*q,DXGI_SWAP_CHAIN_DESC*d,IDXGISwapChain**out){auto hr=backend().create(f,q,d,out);if(SUCCEEDED(hr))backend().adopt(*out,q);return hr;}
HRESULT STDMETHODCALLTYPE on_create_hwnd(IDXGIFactory2*f,IUnknown*q,HWND w,const DXGI_SWAP_CHAIN_DESC1*d,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*full,IDXGIOutput*o,IDXGISwapChain1**out){auto hr=backend().create_hwnd(f,q,w,d,full,o,out);if(SUCCEEDED(hr))backend().adopt(*out,q);return hr;}
}
extern "C" int tm_render_start(void(*emergency)()){
 auto&b=backend();b.emergency=emergency;ComPtr<ID3D12Device>d;ComPtr<ID3D12CommandQueue>q;ComPtr<IDXGIFactory4>factory;ComPtr<IDXGISwapChain>swap;
 HWND dummy=CreateWindowExW(0,L"STATIC",L"Theater DX12 discovery",WS_POPUP,0,0,1,1,nullptr,nullptr,nullptr,nullptr);if(!dummy)return 0;
 D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=2;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=dummy;sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
 bool ok=SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&d)))&&SUCCEEDED(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)))&&SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateSwapChain(q.Get(),&sd,&swap));
 if(ok){auto s=*reinterpret_cast<void***>(swap.Get());auto f=*reinterpret_cast<void***>(factory.Get());auto status=MH_Initialize();ok=status==MH_OK||status==MH_ERROR_ALREADY_INITIALIZED;
  struct Hook{void*target,*detour;void**original;};Hook hooks[]{{s[8],reinterpret_cast<void*>(on_present),reinterpret_cast<void**>(&b.present)},{s[13],reinterpret_cast<void*>(on_resize),reinterpret_cast<void**>(&b.resize)},{f[10],reinterpret_cast<void*>(on_create),reinterpret_cast<void**>(&b.create)},{f[15],reinterpret_cast<void*>(on_create_hwnd),reinterpret_cast<void**>(&b.create_hwnd)},{reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetCursorPos")),reinterpret_cast<void*>(on_set_cursor_pos),reinterpret_cast<void**>(&b.set_cursor_pos)}};
  for(auto&hook:hooks){if(!ok)break;ok=MH_CreateHook(hook.target,hook.detour,hook.original)==MH_OK;if(ok)b.targets.push_back(hook.target);}
  if(ok)for(auto*t:b.targets)if(MH_EnableHook(t)!=MH_OK){ok=false;break;}
  if(!ok){for(auto*t:b.targets){MH_DisableHook(t);MH_RemoveHook(t);}b.targets.clear();}
 }DestroyWindow(dummy);
 if(ok){b.client=std::thread([&b]{b.ipc_worker();});b.log("DX12_HOOKS_INSTALLED; waiting for native swapchain creation (restart required for late injection)");}else b.log("DX12_HOOK_INSTALL_FAILED; native replay remains independent");return ok?1:0;
}
extern "C" void tm_render_shutdown(){auto&b=backend();b.running=false;if(b.client.joinable()){CancelSynchronousIo(b.client.native_handle());b.client.join();}
 for(auto*t:b.targets)MH_DisableHook(t);std::lock_guard lock(b.graphics);if(b.previous_proc&&IsWindow(b.hwnd)&&reinterpret_cast<WNDPROC>(GetWindowLongPtrW(b.hwnd,GWLP_WNDPROC))==TheaterRenderBackend::wndproc)SetWindowLongPtrW(b.hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(b.previous_proc));
 if(b.wait_gpu())b.release_resources();for(auto*t:b.targets)MH_RemoveHook(t);b.targets.clear();b.chain.Reset();b.queue.Reset();
}
// CPU-only UI construction test. Does not install hooks or assert game rendering.
extern "C" int tm_render_test_ui(){
 auto&b=backend();auto*c=ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;io.Fonts->AddFontDefault();
 unsigned char*p=nullptr;int w=0,h=0;io.Fonts->GetTexDataAsRGBA32(&p,&w,&h);
 {std::lock_guard lock(b.ipc);b.snapshot.loaded=1;b.snapshot.connected=1;b.snapshot.player_found=1;b.snapshot.duration_ns=10'000'000'000;b.snapshot.count=16;
  for(unsigned i=0;i<16;++i)b.snapshot.actors[i].id=i+1;}
 bool valid=true;for(auto size:{ImVec2{1280,720},ImVec2{3440,1440},ImVec2{7680,2160}})for(int mode:{1,2}){
  b.mode=mode;io.DisplaySize=size;ImGui::NewFrame();b.draw();ImGui::Render();valid=valid&&ImGui::GetDrawData()->Valid;
 }ImGui::DestroyContext(c);return valid?1:0;
}
