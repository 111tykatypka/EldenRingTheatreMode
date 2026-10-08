// Independent DX12 backend. Design references: FreecamMod and dx12-imgui-overlay.
// No game offsets or character writes are implemented in this translation unit.
#include "NativeLightBackend.h"
#include "NativeParticleBackend.h"
#include "ColorGrading.h"
#include <windows.h>
#include "EldenRingTimingAdapter.h"
#include "EldenRingWeatherAdapter.h"
#include "EldenRingLightAdapter.h"
#include "LightEditor.h"
#include "EldenRingHudAdapter.h"
#include <realtimeapiset.h>
#pragma comment(lib,"mincore.lib")
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include "TheaterUiProtocol.h"
#include "theater/TheaterOverlayUI.h"
#include "TheaterHotkeys.h"
#include "CinematicCameraRuntime.h"
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#include <fstream>
#include <filesystem>
using Microsoft::WRL::ComPtr;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
std::mutex camera_mutex;
theater_camera::Telemetry camera_snapshot;
std::deque<theater_hotkeys::Action> camera_actions;
using Present=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using Resize=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
using Create=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using CreateHwnd=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**);
struct Frame{ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12Resource> scene;D3D12_GPU_DESCRIPTOR_HANDLE scene_srv{};bool scene_failed=false;ComPtr<ID3D12Resource> buffer;D3D12_CPU_DESCRIPTOR_HANDLE rtv{};UINT64 fence{};};
struct Input{HWND hwnd;UINT msg;WPARAM w;LPARAM l;};
class TheaterRenderBackend {
public:
 color_grading::Pass grading_pass;
 ULONGLONG grading_output_checked=0;bool grading_output_sdr=false;
 bool sdr_output(){
  const auto now=GetTickCount64();if(grading_output_checked&&now-grading_output_checked<1000)return grading_output_sdr;
  grading_output_checked=now;grading_output_sdr=false;
  ComPtr<IDXGIOutput> output;ComPtr<IDXGIOutput6> output6;DXGI_OUTPUT_DESC1 description{};
  if(SUCCEEDED(chain->GetContainingOutput(&output))&&SUCCEEDED(output.As(&output6))&&SUCCEEDED(output6->GetDesc1(&description)))
   grading_output_sdr=description.ColorSpace==DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
  return grading_output_sdr;
 }
 std::recursive_mutex graphics;std::mutex ipc,input_mutex;
 // Messages from the game side (bone replay) for the overlay event log; drained by draw().
 std::mutex events_mutex;std::deque<std::string> events;
 BOOL (WINAPI* set_cursor_pos)(int,int){};BOOL (WINAPI* clip_cursor)(const RECT*){};Present present{};Resize resize{};Create create{};CreateHwnd create_hwnd{};
 std::vector<void*> targets;ComPtr<IDXGISwapChain3> chain;ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12Device> device;
 ComPtr<ID3D12DescriptorHeap> rtvs,srvs;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
 std::vector<Frame> frames;UINT64 fence_value{};HANDLE fence_event{};HWND hwnd{};WNDPROC previous_proc{};
 ImGuiContext* context{};bool win32_ready{},dx12_ready{},failed{};std::vector<bool> descriptors;
 std::deque<Input> inputs;std::deque<theater_ui::Request> commands;theater_ui::Snapshot snapshot;
 // mode 2 = v3 UI shown (cursor + input capture), 0 = hidden. visibility holds TheaterUI::UiVisibility.
 std::atomic_int mode{0},visibility{int(TheaterUI::UiVisibility::Hidden)};std::atomic<ULONGLONG> hidden_tick{0};
 std::atomic_bool running{true},ui_ready{},capture_mouse{},capture_keyboard{},host_linked{};std::thread client;
 TheaterUI::Overlay overlay;TheaterUI::UiVisibility before_clean=TheaterUI::UiVisibility::Hidden;bool all_hidden=false;
 // Game input is blocked only while the UI is shown AND actually rendering, so a failed overlay never traps the player.
 bool focused() const {auto fg=GetForegroundWindow();return hwnd&&fg&&GetAncestor(fg,GA_ROOT)==GetAncestor(hwnd,GA_ROOT);}
 bool blocking() const {return focused()&&mode.load()==2&&ui_ready.load()&&!failed;}
 // Set by bone replay playback: the player character is the replay body, so the game gets no
 // keyboard and no mouse buttons (mouse movement still turns the camera). Gamepads are not blocked yet.
 std::atomic_bool game_input_locked{};
 // When the latest host snapshot arrived, in the game's monotonic clock (QueryInterruptTimePrecise, ns).
 std::atomic<std::uint64_t> snapshot_ns{};
 // Replay state from the latest host snapshot, for keys that only act while a replay is loaded.
 std::atomic_bool replay_loaded{},replay_playing{};
 // TogglePlayback (Space) belongs to Theater Mode only while the overlay is open (game input is
 // blocked then anyway). With the overlay hidden it is always the game's key, replay loaded or not.
 bool owns_playback_key() const {return focused()&&(blocking()||replay_loaded.load());}
 void toggle_playback(){if(replay_loaded.load())command(theater_ui::toggle_playback);}
 // Virtual cursor. Elden Ring can hold the mouse through DirectInput so Windows never moves
 // the cursor or sends WM_MOUSEMOVE. While the UI is shown, the blocked game mouse deltas and
 // buttons drive the ImGui cursor instead, unless real window mouse messages are arriving.
 std::atomic<long> mouse_dx{},mouse_dy{};std::atomic<unsigned> mouse_buttons{};std::atomic<ULONGLONG> os_mouse_tick{};
 ImVec2 virtual_mouse{-1,-1};unsigned applied_buttons{};
 // Exactly one cursor source per frame, so position and clicks never disagree.
 //  - Windows mode (default): the game keeps sending window mouse messages, so ImGui gets
 //    position and buttons only from those (the queued WM_* events). Nothing else moves it.
 //  - DirectInput mode: only if no window mouse message arrived for 1 s while shown (the game
 //    holds the mouse exclusively). Then the blocked DirectInput deltas and buttons drive it.
 // The OS cursor is hidden while shown (WM_SETCURSOR) and the overlay draws the only arrow.
 ULONGLONG cursor_log_tick{};bool dinput_mode{};
 void feed_virtual_mouse(){auto&io=ImGui::GetIO();const long dx=mouse_dx.exchange(0),dy=mouse_dy.exchange(0);const unsigned buttons=mouse_buttons.load();
  if(!focused()||mode.load()!=2){virtual_mouse={-1,-1};applied_buttons=0;dinput_mode=false;return;}
  const ULONGLONG now=GetTickCount64();const bool dinput=now-os_mouse_tick.load()>1000;
  if(dinput!=dinput_mode){dinput_mode=dinput;virtual_mouse=ImGui::IsMousePosValid(&io.MousePos)?io.MousePos:ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.5f);
   log(dinput?"CURSOR source=DirectInput (no window mouse messages for 1 s)":"CURSOR source=Windows messages");}
  if(now-cursor_log_tick>5000){cursor_log_tick=now;char line[160];snprintf(line,sizeof(line),"CURSOR source=%s pos=(%.0f,%.0f) display=(%.0f,%.0f)",dinput?"DirectInput":"Windows",io.MousePos.x,io.MousePos.y,io.DisplaySize.x,io.DisplaySize.y);log(line);}
  if(!dinput){applied_buttons=buttons;return;}
  virtual_mouse.x=std::clamp(virtual_mouse.x+float(dx),0.f,std::max(0.f,io.DisplaySize.x-1));virtual_mouse.y=std::clamp(virtual_mouse.y+float(dy),0.f,std::max(0.f,io.DisplaySize.y-1));
  io.AddMousePosEvent(virtual_mouse.x,virtual_mouse.y);
  for(unsigned i=0;i<3;++i){const bool down=(buttons>>i)&1;if(down!=(((applied_buttons>>i)&1)!=0))io.AddMouseButtonEvent(int(i),down);}applied_buttons=buttons;}
 std::atomic<WNDPROC> forward_proc{};void* exception_observer{};unsigned diagnostic_frames{};
 void (*emergency)(){};
 void log(const char*message){wchar_t path[MAX_PATH]{};GetTempPathW(MAX_PATH,path);std::ofstream file(std::filesystem::path(path)/L"TheaterModeRender.log",std::ios::app);file<<GetTickCount64()<<" "<<message<<'\n';}
 void command(std::uint32_t kind,std::uint64_t value=0,const char*text=nullptr){if(kind==TheaterUI::kCommandCleanView){clean_view();return;}if(kind==TheaterUI::kCommandToggleUi){toggle_ui(false);return;}
  if(kind==TheaterUI::kCommandSetVisibility){set_visibility(value==0?TheaterUI::UiVisibility::Shown:TheaterUI::UiVisibility::Hidden);return;}
  if(kind==theater_ui::play||kind==theater_ui::restart||kind==theater_ui::toggle_playback)game_timing::enable(true);
  if(kind==theater_ui::stop){game_timing::enable(false);camera_runtime::stop();native_particles::stop_preview();if(emergency)emergency();}std::lock_guard lock(ipc);
  // A scrub produces many seeks and the pipe sends one request per round trip, so only the newest pending seek matters.
  if(kind==theater_ui::seek&&!commands.empty()&&commands.back().command==theater_ui::seek){commands.back().value=value;return;}
  if(commands.size()<32){theater_ui::Request r;r.command=kind;r.value=value;if(text)strncpy_s(r.text,text,_TRUNCATE);commands.push_back(r);}}
 void clean_view(){if(!all_hidden){if(!game_hud::ready()){log("HUD_OPACITY unavailable; clean toggle rejected");return;}before_clean=TheaterUI::UiVisibility(visibility.load());all_hidden=true;game_hud::request(true);set_visibility(TheaterUI::UiVisibility::HiddenClean);}else{all_hidden=false;game_hud::request(false);set_visibility(before_clean);}}
 void set_visibility(TheaterUI::UiVisibility next){if(next==TheaterUI::UiVisibility::Shown){all_hidden=false;game_hud::request(false);}if(next!=TheaterUI::UiVisibility::Shown)hidden_tick=GetTickCount64();visibility=int(next);mode=next==TheaterUI::UiVisibility::Shown?2:0;
  if(next==TheaterUI::UiVisibility::Shown)ClipCursor(nullptr);}
 // True while an ImGui text field has keyboard focus: F4 and Space then type instead of acting.
 std::atomic_bool text_input{};
 // F4 toggles Shown/Hidden. Shift+F4 toggles the clean mode that also hides the REC pill.
 void toggle_ui(bool clean){using V=TheaterUI::UiVisibility;const auto current=V(visibility.load());V next;
  if(clean)next=current==V::HiddenClean?V::Hidden:V::HiddenClean;else next=current==V::Shown?V::Hidden:V::Shown;
  if(next==V::Shown){all_hidden=false;game_hud::request(false);}
  if(next!=V::Shown)hidden_tick=GetTickCount64();visibility=int(next);mode=next==V::Shown?2:0;
  if(next==V::Shown)ClipCursor(nullptr);
  if((current==V::Shown)!=(next==V::Shown))TheaterUI::Sound::Play(next==V::Shown?TheaterUI::Sound::Cue::Open:TheaterUI::Sound::Cue::Close);} // through the hook: confines to the whole window while shown
 bool transfer(HANDLE h,void*p,DWORD n,bool write){auto*c=static_cast<char*>(p);while(n){DWORD got=0;if(!(write?WriteFile(h,c,n,&got,nullptr):ReadFile(h,c,n,&got,nullptr))||!got)return false;c+=got;n-=got;}return true;}
 void ipc_worker(){HANDLE h=INVALID_HANDLE_VALUE;std::uint64_t sequence=0;while(running){theater_hotkeys::Reload();
  if(h==INVALID_HANDLE_VALUE){h=CreateFileW(theater_ui::pipe,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE){Sleep(100);continue;}}
  theater_ui::Request r;{std::lock_guard lock(ipc);if(!commands.empty()){r=commands.front();commands.pop_front();}}if(r.command==theater_ui::poll)if(auto query=camera_runtime::player_target_query()){r.command=theater_ui::camera_target_window;r.value=*query;}
  r.sequence=++sequence;theater_ui::Snapshot s;
  if(!transfer(h,&r,sizeof(r),true)||!transfer(h,&s,sizeof(s),false)||s.magic_value!=theater_ui::magic||s.version!=theater_ui::version||s.sequence!=r.sequence||s.count>16||s.target_count>64||!theater_timescale::valid(s.timescale)){
   CloseHandle(h);h=INVALID_HANDLE_VALUE;host_linked=false;game_weather::host_connected(false);game_lights::host_connected(false);native_lights::host_connected(false);tm_lighting_time_connected(0);replay_loaded=false;replay_playing=false;camera_runtime::timeline(0,0,0,false,1,false);{std::lock_guard lock(ipc);snapshot={};commands.clear();}Sleep(100);continue;}
  {std::lock_guard lock(ipc);snapshot=s;}{ULONGLONG t=0;QueryInterruptTimePrecise(&t);snapshot_ns=std::uint64_t(t)*100;}host_linked=true;game_weather::host_connected(s.connected&&s.player_found);game_lights::host_connected(s.connected&&s.player_found);native_lights::host_connected(s.connected&&s.player_found);tm_lighting_time_connected(s.connected&&s.player_found);replay_loaded=s.loaded!=0;replay_playing=s.host_playing!=0;
  camera_runtime::timeline(s.time_ns,s.duration_ns,s.master_clock_ns,s.host_playing!=0,s.timescale,true,s.loaded_path);
  std::vector<camera_runtime::TargetSample> targets;for(unsigned i=0;i<s.target_count;++i){const auto&p=s.target_points[i];targets.push_back({p.time_ns,{p.position[0],p.position[1],p.position[2]}});}camera_runtime::player_targets(targets);Sleep(50);
 }game_weather::host_connected(false);game_lights::host_connected(false);native_lights::host_connected(false);tm_lighting_time_connected(0);if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
 void adopt(IDXGISwapChain*sc,IUnknown*unknown){ComPtr<ID3D12CommandQueue> q;ComPtr<IDXGISwapChain3> c;
  if(FAILED(unknown->QueryInterface(IID_PPV_ARGS(&q)))||q->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT||FAILED(sc->QueryInterface(IID_PPV_ARGS(&c))))return;
  std::lock_guard lock(graphics);if(chain)return;chain=c;queue=q;
  DXGI_SWAP_CHAIN_DESC desc{};if(FAILED(chain->GetDesc(&desc))||!IsWindow(desc.OutputWindow)){failed=true;log("DX12_INVALID_WINDOW");return;}
  hwnd=desc.OutputWindow;
  camera_runtime::window(hwnd);
  // Publish the forwarding target BEFORE another thread can enter our WndProc.
  // The former SetWindowLongPtr assignment published it only after installation.
  auto old=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd,GWLP_WNDPROC));
  if(!old){failed=true;log("DX12_WNDPROC_READ_FAILED");return;}forward_proc.store(old,std::memory_order_release);
  SetLastError(0);auto replaced=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(wndproc)));
  if(!replaced){failed=true;log("DX12_WNDPROC_INSTALL_FAILED");return;}previous_proc=replaced;forward_proc.store(replaced,std::memory_order_release);
  char detail[256];snprintf(detail,sizeof(detail),"DX12_QUEUE_BOUND format=%u buffers=%u hwnd=%p; CLEAN startup; Insert enables experimental UI",unsigned(desc.BufferDesc.Format),desc.BufferCount,hwnd);log(detail);}
 bool wait_gpu(){if(!fence||!queue)return true;if(FAILED(queue->Signal(fence.Get(),++fence_value)))return false;
  if(fence->GetCompletedValue()>=fence_value)return true;
  return SUCCEEDED(fence->SetEventOnCompletion(fence_value,fence_event))&&WaitForSingleObject(fence_event,2000)==WAIT_OBJECT_0;}
 void release_resources(){ui_ready=false;if(context){ImGui::SetCurrentContext(context);if(dx12_ready)ImGui_ImplDX12_Shutdown();if(win32_ready)ImGui_ImplWin32_Shutdown();ImGui::DestroyContext(context);}
  grading_output_checked=0;grading_pass.reset();color_grading::report(false,"Renderer resources released");
  context=nullptr;diagnostic_frames=0;dx12_ready=win32_ready=false;frames.clear();list.Reset();rtvs.Reset();srvs.Reset();fence.Reset();device.Reset();descriptors.clear();if(fence_event){CloseHandle(fence_event);fence_event=nullptr;}}
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
  context=ImGui::CreateContext();ImGui::SetCurrentContext(context);auto&io=ImGui::GetIO();io.IniFilename=nullptr;
  // The v3 theme scales from the swap-chain size (TheaterLayout.h), not from window DPI.
  overlay.Init(io);
  win32_ready=ImGui_ImplWin32_Init(hwnd);if(!win32_ready)return false;
  ImGui_ImplDX12_InitInfo info;info.Device=device.Get();info.CommandQueue=queue.Get();info.NumFramesInFlight=static_cast<int>(desc.BufferCount);info.RTVFormat=desc.BufferDesc.Format;info.SrvDescriptorHeap=srvs.Get();info.UserData=this;
  info.SrvDescriptorAllocFn=[](ImGui_ImplDX12_InitInfo*i,D3D12_CPU_DESCRIPTOR_HANDLE*c,D3D12_GPU_DESCRIPTOR_HANDLE*g){auto*b=static_cast<TheaterRenderBackend*>(i->UserData);auto stride=b->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);for(UINT n=0;n<b->descriptors.size();++n)if(!b->descriptors[n]){b->descriptors[n]=true;c->ptr=b->srvs->GetCPUDescriptorHandleForHeapStart().ptr+n*stride;g->ptr=b->srvs->GetGPUDescriptorHandleForHeapStart().ptr+n*stride;return;}*c={};*g={};b->failed=true;};
  info.SrvDescriptorFreeFn=[](ImGui_ImplDX12_InitInfo*i,D3D12_CPU_DESCRIPTOR_HANDLE c,D3D12_GPU_DESCRIPTOR_HANDLE){auto*b=static_cast<TheaterRenderBackend*>(i->UserData);auto stride=b->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);auto index=(c.ptr-b->srvs->GetCPUDescriptorHandleForHeapStart().ptr)/stride;if(index<b->descriptors.size())b->descriptors[index]=false;};
  dx12_ready=ImGui_ImplDX12_Init(&info);if(!dx12_ready)return false;
  ui_ready=true;log("DX12_IMGUI_INITIALIZED; v3 UI; F4 shows/hides, Shift+F4 clean; runtime visuals require user verification");return true;
 }
 bool recording_now(){std::lock_guard lock(ipc);return TheaterUI::Overlay::IsRecording(snapshot);}
 // Builds the v3 UI frame from a snapshot copy.
 // Allocate once per swap-chain slot, reuse after its existing fence completes.
 bool prepare_scene(Frame& f){
  if(f.scene)return true;if(f.scene_failed)return false;
  auto desc=f.buffer->GetDesc();if(desc.SampleDesc.Count!=1){f.scene_failed=true;return false;}desc.Flags=D3D12_RESOURCE_FLAG_NONE;
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
  if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&f.scene)))){f.scene_failed=true;log("GAME_VIEW_COPY_ALLOCATION_FAILED; full-screen rendering retained");return false;}
  auto slot=std::find(descriptors.begin(),descriptors.end(),false);if(slot==descriptors.end()){f.scene.Reset();f.scene_failed=true;return false;}
  auto index=UINT(slot-descriptors.begin());descriptors[index]=true;auto stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  D3D12_CPU_DESCRIPTOR_HANDLE cpu{srvs->GetCPUDescriptorHandleForHeapStart().ptr+index*stride};f.scene_srv={srvs->GetGPUDescriptorHandleForHeapStart().ptr+index*stride};
  D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;device->CreateShaderResourceView(f.scene.Get(),&srv,cpu);return true;
 }
 TheaterUI::LayoutRects draw(ImTextureID scene=0){
  TheaterUI::OverlayFrame frame;{std::lock_guard lock(ipc);frame.snapshot=snapshot;}
  {std::lock_guard lock(camera_mutex);frame.camera=camera_snapshot;}
  {std::lock_guard lock(camera_mutex);while(!camera_actions.empty()){overlay.CameraHotkey(camera_actions.front());camera_actions.pop_front();}}
  frame.focused=focused();frame.game_texture=scene;
  frame.hostLinked=host_linked.load();frame.visibility=TheaterUI::UiVisibility(visibility.load());
  frame.now=double(GetTickCount64())/1000.0;frame.hiddenAt=double(hidden_tick.load())/1000.0;
  {std::lock_guard lock(events_mutex);frame.events.assign(events.begin(),events.end());events.clear();}
  auto&io=ImGui::GetIO();const bool shown=frame.visibility==TheaterUI::UiVisibility::Shown;io.MouseDrawCursor=false; // the overlay draws its own cursor (TheaterOverlayUI)
  camera_runtime::overlay_visible(shown);
  const auto rects=overlay.Draw(frame,[](void*user,std::uint32_t kind,std::uint64_t value,const char*text){static_cast<TheaterRenderBackend*>(user)->command(kind,value,text);},this);
  text_input=shown&&frame.focused&&ImGui::GetIO().WantTextInput;
  capture_mouse=shown&&frame.focused&&io.WantCaptureMouse;capture_keyboard=shown&&frame.focused&&io.WantCaptureKeyboard;return rects;
 }
 void render(IDXGISwapChain*sc,UINT flags){if(flags&DXGI_PRESENT_TEST)return;std::lock_guard lock(graphics);if(failed||!chain||sc!=static_cast<IDXGISwapChain*>(chain.Get()))return;
  // Draw only when something is visible: the UI, the REC pill (not in Shift+F4 clean mode), or the F4 hint.
  const auto vis=TheaterUI::UiVisibility(visibility.load());
  const bool hint=vis==TheaterUI::UiVisibility::Hidden; // permanent "F4 Show UI" hint
  const bool needed=vis==TheaterUI::UiVisibility::Shown||hint||(vis!=TheaterUI::UiVisibility::HiddenClean&&recording_now());
  if(!needed&&!color_grading::settings().enabled&&!context)return;
  const bool trace=diagnostic_frames<3;auto stage=[&](const char*s){if(trace)log(s);};stage("FRAME_BEGIN");
  if(!context&&!initialize_resources()){log("DX12_RESOURCE_INIT_FAILED; overlay disabled; replay integration unchanged");release_resources();failed=true;return;}
  ImGui::SetCurrentContext(context);std::deque<Input> pending;
  {std::lock_guard lock(input_mutex);pending.swap(inputs);}
  // Win32 input dispatch can synchronously re-enter our WndProc (mouse capture).
  // Never hold the queue lock across backend/Win32 calls.
  for(const auto&m:pending){
   int button=-1;bool down=false;
   switch(m.msg){
   case WM_LBUTTONDOWN:case WM_LBUTTONDBLCLK:button=0;down=true;break;case WM_LBUTTONUP:button=0;break;
   case WM_RBUTTONDOWN:case WM_RBUTTONDBLCLK:button=1;down=true;break;case WM_RBUTTONUP:button=1;break;
   case WM_MBUTTONDOWN:case WM_MBUTTONDBLCLK:button=2;down=true;break;case WM_MBUTTONUP:button=2;break;
   case WM_XBUTTONDOWN:case WM_XBUTTONDBLCLK:button=HIWORD(m.w)==XBUTTON1?3:4;down=true;break;
   case WM_XBUTTONUP:button=HIWORD(m.w)==XBUTTON1?3:4;break;
   }
   // The game owns HWND capture. The render thread must not SetCapture/ReleaseCapture
   // on that window. Forward button data to ImGui without the backend's capture calls.
   if(button>=0)ImGui::GetIO().AddMouseButtonEvent(button,down);
   else ImGui_ImplWin32_WndProcHandler(m.hwnd,m.msg,m.w,m.l);
  }
  const auto index=chain->GetCurrentBackBufferIndex();if(index>=frames.size())return;auto&f=frames[index];if(f.fence&&fence->GetCompletedValue()<f.fence)return; // Do not stall game Present.
  const bool composite=vis==TheaterUI::UiVisibility::Shown&&prepare_scene(f);
  stage("FRAME_DX12_NEWFRAME");ImGui_ImplDX12_NewFrame();stage("FRAME_WIN32_NEWFRAME");ImGui_ImplWin32_NewFrame();const bool active=focused();game_hud::focused(active);TheaterUI::Sound::SetFocused(active);
  if(!active){std::lock_guard lock(input_mutex);inputs.clear();mouse_dx=0;mouse_dy=0;mouse_buttons=0;applied_buttons=0;virtual_mouse={-1,-1};ImGui::GetIO().ClearEventsQueue();ImGui::GetIO().ClearInputKeys();ImGui::GetIO().ClearInputMouse();ImGui::GetIO().AddFocusEvent(false);}
  else {ImGui::GetIO().AddFocusEvent(true);feed_virtual_mouse();}
  stage("FRAME_IMGUI_NEWFRAME");ImGui::NewFrame();stage("FRAME_DRAW");draw(composite?ImTextureID(f.scene_srv.ptr):0);
  ImGui::Render();
  const auto gradeValues=color_grading::settings();bool grade=false;
  bool loadedWorld=false;{std::lock_guard lock(ipc);loadedWorld=host_linked.load()&&snapshot.connected&&snapshot.player_found;}
  const auto bufferDesc=f.buffer->GetDesc();
  const bool sdr=sdr_output()&&
      (bufferDesc.Format==DXGI_FORMAT_R8G8B8A8_UNORM||bufferDesc.Format==DXGI_FORMAT_B8G8R8A8_UNORM);
  if(!gradeValues.enabled)color_grading::report(false,"Disabled");
  else if(color_grading::neutral(gradeValues))color_grading::report(true,"Neutral values: color pass bypassed");
  else if(!loadedWorld)color_grading::report(false,"Waiting for connected player / loaded world");
  else if(!active)color_grading::report(false,"Suspended while game is unfocused");
  else if(!sdr)color_grading::report(false,"Unavailable for this swap-chain color space / format (SDR 8-bit only)");
  else if(!prepare_scene(f))color_grading::report(false,"Scene texture unavailable; grading bypassed");
  else if(grading_pass.prepare(device.Get(),bufferDesc.Format)&&grading_pass.prepare_lut(device.Get(),index)){grade=true;color_grading::report(true,"Active: SDR Look effects");}
  if(!needed&&!grade)return;
  if(FAILED(f.allocator->Reset())||FAILED(list->Reset(f.allocator.Get(),nullptr))){failed=true;return;}
  if(composite||grade){
   D3D12_RESOURCE_BARRIER copies[2]{};for(auto&b:copies){b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;}
   copies[0].Transition.pResource=f.buffer.Get();copies[0].Transition.StateBefore=D3D12_RESOURCE_STATE_PRESENT;copies[0].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
   copies[1].Transition.pResource=f.scene.Get();copies[1].Transition.StateBefore=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;copies[1].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;
   list->ResourceBarrier(2,copies);list->CopyResource(f.scene.Get(),f.buffer.Get());
   for(auto&b:copies)std::swap(b.Transition.StateBefore,b.Transition.StateAfter);list->ResourceBarrier(2,copies);
  }
  D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={f.buffer.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET};list->ResourceBarrier(1,&barrier);
  list->OMSetRenderTargets(1,&f.rtv,FALSE,nullptr);ID3D12DescriptorHeap*heap=srvs.Get();list->SetDescriptorHeaps(1,&heap);
  if(grade){
   grading_pass.record(list.Get(),f.scene_srv,UINT(bufferDesc.Width),bufferDesc.Height,gradeValues,index);
   if(composite){
    // The editor viewport must sample the graded image, not the earlier raw scene copy.
    D3D12_RESOURCE_BARRIER copies[2]{};for(auto&b:copies){b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;}
    copies[0].Transition.pResource=f.buffer.Get();copies[0].Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;copies[0].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
    copies[1].Transition.pResource=f.scene.Get();copies[1].Transition.StateBefore=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;copies[1].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;
    list->ResourceBarrier(2,copies);list->CopyResource(f.scene.Get(),f.buffer.Get());
    for(auto&b:copies)std::swap(b.Transition.StateBefore,b.Transition.StateAfter);list->ResourceBarrier(2,copies);
    list->OMSetRenderTargets(1,&f.rtv,FALSE,nullptr);
   }
  }
  stage("FRAME_RENDER_DRAW_DATA");ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),list.Get());std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);list->ResourceBarrier(1,&barrier);
  if(FAILED(list->Close())){failed=true;return;}ID3D12CommandList*cmd=list.Get();stage("FRAME_EXECUTE");queue->ExecuteCommandLists(1,&cmd);f.fence=++fence_value;if(FAILED(queue->Signal(fence.Get(),f.fence))){failed=true;log("DX12_QUEUE_SIGNAL_FAILED; rendering disabled");}stage("FRAME_SUBMITTED");++diagnostic_frames;
 }
 static LRESULT CALLBACK wndproc(HWND,UINT,WPARAM,LPARAM);
};
// Loaded for process lifetime, like the existing recurring Rust task callbacks.
// Dynamic FreeLibrary is not supported; explicit shutdown is outside DllMain.
TheaterRenderBackend&backend(){static auto*b=new TheaterRenderBackend;return *b;}
// Diagnostic only: never swallow an exception or resume a damaged renderer.
// No game memory inspection, registry changes, or global exception-filter replacement.
LONG CALLBACK observe_exception(EXCEPTION_POINTERS*e){
 const DWORD code=e->ExceptionRecord->ExceptionCode;
 if(code!=EXCEPTION_ACCESS_VIOLATION&&code!=EXCEPTION_ILLEGAL_INSTRUCTION&&code!=EXCEPTION_INT_DIVIDE_BY_ZERO&&code!=EXCEPTION_BREAKPOINT)return EXCEPTION_CONTINUE_SEARCH;
 static std::atomic_uint reports{};static thread_local bool busy=false;if(busy||reports.fetch_add(1)>=3)return EXCEPTION_CONTINUE_SEARCH;busy=true;
 wchar_t path[MAX_PATH]{};GetTempPathW(MAX_PATH,path);wcscat_s(path,L"TheaterModeCrash.log");
 HANDLE file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file!=INVALID_HANDLE_VALUE){
  auto emit=[&](void*address,const char*kind){HMODULE module{};wchar_t name[MAX_PATH]{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),&module);if(module)GetModuleFileNameW(module,name,MAX_PATH);
   char line[1200];int n=snprintf(line,sizeof(line),"tick=%llu pid=%lu code=0x%08lX %s=%p module=%ls base=%p rva=0x%llX\r\n",GetTickCount64(),GetCurrentProcessId(),code,kind,address,name,module,static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(address)-reinterpret_cast<std::uintptr_t>(module)));DWORD wrote;if(n>0)WriteFile(file,line,static_cast<DWORD>(std::min(n,static_cast<int>(sizeof(line)-1))),&wrote,nullptr);};
  emit(reinterpret_cast<void*>(e->ContextRecord->Rip),"fault_rip");void*stack[16]{};USHORT count=CaptureStackBackTrace(0,16,stack,nullptr);for(USHORT i=0;i<count;++i)emit(stack[i],"observer_stack");FlushFileBuffers(file);CloseHandle(file);
 }busy=false;return EXCEPTION_CONTINUE_SEARCH;
}
LRESULT CALLBACK TheaterRenderBackend::wndproc(HWND h,UINT m,WPARAM w,LPARAM l){auto&b=backend();
 if(!b.focused()){auto previous=b.forward_proc.load(std::memory_order_acquire);return previous?CallWindowProcW(previous,h,m,w,l):DefWindowProcW(h,m,w,l);}
 if(m==WM_KEYDOWN&&!(l&(1LL<<30))&&w==theater_hotkeys::Key(theater_hotkeys::Action::ToggleAllHud)&&!b.text_input&&!theater_hotkeys::rebinding){b.clean_view();return 0;}
 // F4 replaces Insert. Alt+F4 arrives as WM_SYSKEYDOWN and still closes the game.
 using theater_hotkeys::Action;using theater_hotkeys::Key;
 if((m==WM_SYSKEYDOWN||m==WM_SYSKEYUP)&&w=='Z'&&!b.text_input.load()&&!theater_hotkeys::rebinding&&(GetKeyState(VK_MENU)&0x8000)){
  if(m==WM_SYSKEYDOWN&&!(l&(1LL<<30))){if(GetKeyState(VK_SHIFT)&0x8000)camera_runtime::redo();else camera_runtime::undo();}return 0;
 }
 if((m==WM_KEYDOWN||m==WM_KEYUP)&&!b.text_input.load()&&!theater_hotkeys::rebinding){
  for(auto action:{Action::CycleCamera,Action::AddDollyKey,Action::ClearDollyKeys,Action::ToggleDollyControls,Action::PlayDollyPath})if(w==Key(action)){
   if(m==WM_KEYDOWN&&!(l&(1LL<<30))){
    {std::lock_guard lock(camera_mutex);if(camera_actions.size()<32)camera_actions.push_back(action);}
    if(action==Action::ClearDollyKeys)b.set_visibility(TheaterUI::UiVisibility::Shown);
   }return 0;
  }
 }
 if(m==WM_KEYDOWN&&!(l&(1LL<<30))&&w==Key(Action::ToggleOverlay)&&!b.text_input.load()&&!theater_hotkeys::rebinding){b.toggle_ui((GetKeyState(VK_SHIFT)&0x8000)!=0);return 0;}
 if(m==WM_KEYDOWN&&w==Key(Action::StopRecording)){b.command(theater_ui::stop);return 0;}
 if((m==WM_KEYDOWN||m==WM_KEYUP||m==WM_CHAR)&&w==Key(Action::TogglePlayback)&&b.owns_playback_key()&&!b.text_input.load()&&!theater_hotkeys::rebinding){
  if(m==WM_KEYDOWN&&!(l&(1LL<<30)))b.toggle_playback();return 0;}
 // Use one wheel source (Win32) to avoid counting DirectInput buffered/state copies twice.
 // Shown UI retains its wheel for child scrolling; only hidden Free Camera consumes it.
 if(m==WM_MOUSEWHEEL&&!b.blocking()&&camera_runtime::owns_input()){
  if(camera_runtime::mouse_wheel(double(GET_WHEEL_DELTA_WPARAM(w))/WHEEL_DELTA))return 0;
 }
 const bool input=(m>=WM_MOUSEFIRST&&m<=WM_MOUSELAST)||(m>=WM_KEYFIRST&&m<=WM_KEYLAST)||m==WM_SETFOCUS||m==WM_KILLFOCUS||m==WM_MOUSELEAVE||m==WM_NCMOUSEMOVE||m==WM_NCMOUSELEAVE;
 if(m==WM_MOUSEMOVE)b.os_mouse_tick=GetTickCount64();
 // The overlay draws the only cursor while shown; keep the Windows cursor hidden and the game out of it.
 if(m==WM_SETCURSOR&&b.blocking()){SetCursor(nullptr);return TRUE;}
 if(b.mode.load()&&input){std::lock_guard lock(b.input_mutex);if(b.inputs.size()<256)b.inputs.push_back({h,m,w,l});}
 // While the UI is shown the game gets no mouse or keyboard at all; ImGui already has its copy above.
 // WM_SYS* keys still pass, so Alt+F4 and Alt+Tab keep working.
 if(b.blocking()&&((m>=WM_MOUSEFIRST&&m<=WM_MOUSELAST)||m==WM_KEYDOWN||m==WM_KEYUP||m==WM_CHAR))return 0;
 if((b.game_input_locked.load()||camera_runtime::owns_input())&&(m==WM_KEYDOWN||m==WM_KEYUP||m==WM_CHAR||(m>=WM_LBUTTONDOWN&&m<=WM_MBUTTONDBLCLK)))return 0;
 auto previous=b.forward_proc.load(std::memory_order_acquire);return previous?CallWindowProcW(previous,h,m,w,l):DefWindowProcW(h,m,w,l);
}
// Elden Ring reads keyboard and mouse through DirectInput8 (DINPUT8.dll import), not window
// messages. While the UI is shown, the game's devices still poll normally but report no input.
using GetDeviceState=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,LPVOID);
using GetDeviceData=HRESULT(STDMETHODCALLTYPE*)(IDirectInputDevice8W*,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD);
GetDeviceState original_state[2]{};GetDeviceData original_data[2]{};
// DIMOUSESTATE (16 bytes) or DIMOUSESTATE2 (20 bytes): lX, lY, lZ, then button bytes.
void capture_mouse_state(const void*data,DWORD size){auto&b=backend();if(size!=sizeof(DIMOUSESTATE)&&size!=sizeof(DIMOUSESTATE2))return;
 const auto*m=static_cast<const DIMOUSESTATE*>(data);b.mouse_dx+=m->lX;b.mouse_dy+=m->lY;
 b.mouse_buttons=(m->rgbButtons[0]&0x80?1u:0u)|(m->rgbButtons[1]&0x80?2u:0u)|(m->rgbButtons[2]&0x80?4u:0u);}
// Buffered data has no size hint, so the device type is asked once per device and cached.
DWORD device_type(IDirectInputDevice8W*d){static std::mutex lock;static std::vector<std::pair<void*,DWORD>> known;std::lock_guard g(lock);
 for(auto&k:known)if(k.first==d)return k.second;DIDEVCAPS caps{};caps.dwSize=sizeof(caps);const DWORD type=SUCCEEDED(d->GetCapabilities(&caps))?GET_DIDEVICE_TYPE(caps.dwDevType):0;
 if(known.size()<16)known.push_back({d,type});return type;}
bool is_mouse(IDirectInputDevice8W*d){return device_type(d)==DI8DEVTYPE_MOUSE;}
// DirectInput keyboard offsets are scan codes; derived from the bound virtual key, so a rebind follows.
DWORD playback_scan_code(){return MapVirtualKeyW(theater_hotkeys::Key(theater_hotkeys::Action::TogglePlayback),MAPVK_VK_TO_VSC);}
void capture_mouse_data(IDirectInputDevice8W*d,DWORD object_size,const DIDEVICEOBJECTDATA*data,DWORD count){auto&b=backend();if(!data||object_size<sizeof(DIDEVICEOBJECTDATA)||!is_mouse(d))return;
 auto*bytes=reinterpret_cast<const unsigned char*>(data);for(DWORD i=0;i<count;++i){auto&e=*reinterpret_cast<const DIDEVICEOBJECTDATA*>(bytes+size_t(i)*object_size);
  if(e.dwOfs==DIMOFS_X)b.mouse_dx+=LONG(e.dwData);else if(e.dwOfs==DIMOFS_Y)b.mouse_dy+=LONG(e.dwData);
  else if(e.dwOfs>=DIMOFS_BUTTON0&&e.dwOfs<=DIMOFS_BUTTON2){const unsigned bit=1u<<(e.dwOfs-DIMOFS_BUTTON0);if(e.dwData&0x80)b.mouse_buttons|=bit;else b.mouse_buttons&=~bit;}}}
template<int N> HRESULT STDMETHODCALLTYPE on_device_state(IDirectInputDevice8W*d,DWORD size,LPVOID data){
 const HRESULT hr=original_state[N](d,size,data);if(FAILED(hr)||!data)return hr;auto&b=backend();if(!b.focused())return hr;
 if(camera_runtime::owns_input()&&!b.blocking()&&(size==sizeof(DIMOUSESTATE)||size==sizeof(DIMOUSESTATE2))){auto*m=static_cast<DIMOUSESTATE*>(data);camera_runtime::mouse_delta(m->lX,m->lY);}
 if(b.blocking()){capture_mouse_state(data,size);memset(data,0,size);}
 else if(b.game_input_locked.load()||camera_runtime::owns_input()){if(size==256)memset(data,0,size);else if(size==sizeof(DIMOUSESTATE)||size==sizeof(DIMOUSESTATE2))memset(static_cast<DIMOUSESTATE*>(data)->rgbButtons,0,size-12);}
 else if(size==256&&b.owns_playback_key()){const DWORD code=playback_scan_code();if(code&&code<256)static_cast<BYTE*>(data)[code]=0;}
 return hr;}
template<int N> HRESULT STDMETHODCALLTYPE on_device_data(IDirectInputDevice8W*d,DWORD object_size,LPDIDEVICEOBJECTDATA data,LPDWORD count,DWORD flags){
 const HRESULT hr=original_data[N](d,object_size,data,count,flags);if(FAILED(hr)||!count)return hr;auto&b=backend();if(!b.focused())return hr;
 if(camera_runtime::owns_input()&&!b.blocking()&&data&&!(flags&DIGDD_PEEK)&&object_size>=sizeof(DIDEVICEOBJECTDATA)&&is_mouse(d)){
  auto*bytes=reinterpret_cast<const unsigned char*>(data);for(DWORD i=0;i<*count;++i){auto&e=*reinterpret_cast<const DIDEVICEOBJECTDATA*>(bytes+size_t(i)*object_size);if(e.dwOfs==DIMOFS_X)camera_runtime::mouse_delta(LONG(e.dwData),0);else if(e.dwOfs==DIMOFS_Y)camera_runtime::mouse_delta(0,LONG(e.dwData));}
 }
 if(b.blocking()){if(!(flags&DIGDD_PEEK))capture_mouse_data(d,object_size,data,*count);*count=0;}
 else if((b.game_input_locked.load()||camera_runtime::owns_input())&&data&&object_size>=sizeof(DIDEVICEOBJECTDATA)){
  // Keep only mouse movement (camera); drop keys and mouse buttons.
  const bool mouse=is_mouse(d);auto*bytes=reinterpret_cast<unsigned char*>(data);DWORD kept=0;
  for(DWORD i=0;i<*count;++i){auto*e=reinterpret_cast<DIDEVICEOBJECTDATA*>(bytes+size_t(i)*object_size);
   if(!mouse||e->dwOfs>=DIMOFS_BUTTON0)continue;if(kept!=i)memmove(bytes+size_t(kept)*object_size,e,object_size);++kept;}
  *count=kept;}
 else if(data&&object_size>=sizeof(DIDEVICEOBJECTDATA)&&b.owns_playback_key()&&device_type(d)==DI8DEVTYPE_KEYBOARD){
  // Drop only the playback key's events, keep the rest in order.
  const DWORD code=playback_scan_code();auto*bytes=reinterpret_cast<unsigned char*>(data);DWORD kept=0;
  for(DWORD i=0;i<*count;++i){auto*e=reinterpret_cast<DIDEVICEOBJECTDATA*>(bytes+size_t(i)*object_size);if(e->dwOfs==code)continue;
   if(kept!=i)memmove(bytes+size_t(kept)*object_size,e,object_size);++kept;}
  *count=kept;}
 return hr;}
// Hooks the shared device vtables through throwaway devices. Failure leaves the overlay working
// with window-message blocking only, and is logged.
void install_dinput_hooks(){auto&b=backend();
 auto module=LoadLibraryW(L"dinput8.dll");auto create=module?reinterpret_cast<decltype(&DirectInput8Create)>(GetProcAddress(module,"DirectInput8Create")):nullptr;
 if(!create){b.log("DINPUT_HOOKS_SKIPPED: dinput8 unavailable; game input blocking uses window messages only");return;}
 static const GUID iid_w{0xBF798031,0x483A,0x4DA2,{0xAA,0x99,0x5D,0x64,0xED,0x36,0x97,0x00}},iid_a{0xBF798030,0x483A,0x4DA2,{0xAA,0x99,0x5D,0x64,0xED,0x36,0x97,0x00}};
 static const GUID keyboard{0x6F1D2B61,0xD5A0,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
 void* state_targets[2]{};void* data_targets[2]{};const GUID* iids[2]{&iid_w,&iid_a};
 for(int i=0;i<2;++i){IUnknown*input=nullptr;if(FAILED(create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,*iids[i],reinterpret_cast<void**>(&input),nullptr))||!input)continue;
  IDirectInputDevice8W*device=nullptr;if(SUCCEEDED(reinterpret_cast<IDirectInput8W*>(input)->CreateDevice(keyboard,&device,nullptr))&&device){
   auto vt=*reinterpret_cast<void***>(device);state_targets[i]=vt[9];data_targets[i]=vt[10];device->Release();}
  input->Release();}
 int installed=0;
 auto hook=[&](void*target,void*detour,void**original){if(!target)return;for(auto*t:b.targets)if(t==target)return; // A and W can share one implementation
  if(MH_CreateHook(target,detour,original)==MH_OK){if(MH_EnableHook(target)==MH_OK){b.targets.push_back(target);++installed;}else MH_RemoveHook(target);}};
 hook(state_targets[0],reinterpret_cast<void*>(on_device_state<0>),reinterpret_cast<void**>(&original_state[0]));
 hook(state_targets[1],reinterpret_cast<void*>(on_device_state<1>),reinterpret_cast<void**>(&original_state[1]));
 hook(data_targets[0],reinterpret_cast<void*>(on_device_data<0>),reinterpret_cast<void**>(&original_data[0]));
 hook(data_targets[1],reinterpret_cast<void*>(on_device_data<1>),reinterpret_cast<void**>(&original_data[1]));
 char line[160];snprintf(line,sizeof(line),"DINPUT_HOOKS installed=%d; game keyboard/mouse report no input while the UI is shown",installed);b.log(line);}
// While shown, the game may not confine the cursor to a small region; the overlay needs the
// whole window. Its own clip rectangle is applied again when the UI hides (next game call).
BOOL WINAPI on_clip_cursor(const RECT*r){auto&b=backend();if(b.blocking()&&b.hwnd){RECT client{};POINT tl{0,0},br{};
 if(GetClientRect(b.hwnd,&client)){br={client.right,client.bottom};ClientToScreen(b.hwnd,&tl);ClientToScreen(b.hwnd,&br);RECT whole{tl.x,tl.y,br.x,br.y};return b.clip_cursor(&whole);}}
 return b.clip_cursor(r);}
BOOL WINAPI on_set_cursor_pos(int x,int y){auto&b=backend();if(b.mode.load()==2&&b.ui_ready.load())return TRUE;return b.set_cursor_pos(x,y);}
HRESULT STDMETHODCALLTYPE on_present(IDXGISwapChain*s,UINT interval,UINT flags){try{backend().render(s,flags);}catch(...){std::lock_guard lock(backend().graphics);backend().failed=true;backend().log("DX12_RENDER_EXCEPTION; overlay disabled; forwarding original Present");}return backend().present(s,interval,flags);}
HRESULT STDMETHODCALLTYPE on_resize(IDXGISwapChain*s,UINT n,UINT w,UINT h,DXGI_FORMAT f,UINT flags){auto&b=backend();std::lock_guard lock(b.graphics);
 if(b.chain&&s==static_cast<IDXGISwapChain*>(b.chain.Get())&&b.context){if(!b.wait_gpu()){b.failed=true;b.log("DX12_RESIZE_GPU_TIMEOUT; resources retained; resize refused safely");return DXGI_ERROR_WAS_STILL_DRAWING;}b.release_resources();b.log("DX12_RESIZE: resources released; recreate on next Present");}
 return b.resize(s,n,w,h,f,flags);
}
HRESULT STDMETHODCALLTYPE on_create(IDXGIFactory*f,IUnknown*q,DXGI_SWAP_CHAIN_DESC*d,IDXGISwapChain**out){auto hr=backend().create(f,q,d,out);if(SUCCEEDED(hr))backend().adopt(*out,q);return hr;}
HRESULT STDMETHODCALLTYPE on_create_hwnd(IDXGIFactory2*f,IUnknown*q,HWND w,const DXGI_SWAP_CHAIN_DESC1*d,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*full,IDXGIOutput*o,IDXGISwapChain1**out){auto hr=backend().create_hwnd(f,q,w,d,full,o,out);if(SUCCEEDED(hr))backend().adopt(*out,q);return hr;}
}
// A line for the overlay event log (Debug panel), e.g. bone replay status. Bounded; the oldest lines drop.
extern "C" void tm_render_event(const char* text){auto&b=backend();std::lock_guard lock(b.events_mutex);b.events.emplace_back(text?text:"");while(b.events.size()>256)b.events.pop_front();}
extern "C" int tm_render_start(void(*emergency)()){
 auto&b=backend();b.emergency=emergency;b.exception_observer=AddVectoredExceptionHandler(0,observe_exception);b.log("DIAGNOSTIC_BUILD CLEAN startup; UI opt-in; exception observer does not handle faults");ComPtr<ID3D12Device>d;ComPtr<ID3D12CommandQueue>q;ComPtr<IDXGIFactory4>factory;ComPtr<IDXGISwapChain>swap;
 HWND dummy=CreateWindowExW(0,L"STATIC",L"Theater DX12 discovery",WS_POPUP,0,0,1,1,nullptr,nullptr,nullptr,nullptr);if(!dummy)return 0;
 D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;DXGI_SWAP_CHAIN_DESC sd{};sd.BufferCount=2;sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.OutputWindow=dummy;sd.SampleDesc.Count=1;sd.Windowed=TRUE;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
 bool ok=SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&d)))&&SUCCEEDED(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)))&&SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->CreateSwapChain(q.Get(),&sd,&swap));
 if(ok){auto s=*reinterpret_cast<void***>(swap.Get());auto f=*reinterpret_cast<void***>(factory.Get());auto status=MH_Initialize();ok=status==MH_OK||status==MH_ERROR_ALREADY_INITIALIZED;
  struct Hook{void*target,*detour;void**original;};Hook hooks[]{{s[8],reinterpret_cast<void*>(on_present),reinterpret_cast<void**>(&b.present)},{s[13],reinterpret_cast<void*>(on_resize),reinterpret_cast<void**>(&b.resize)},{f[10],reinterpret_cast<void*>(on_create),reinterpret_cast<void**>(&b.create)},{f[15],reinterpret_cast<void*>(on_create_hwnd),reinterpret_cast<void**>(&b.create_hwnd)},{reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetCursorPos")),reinterpret_cast<void*>(on_set_cursor_pos),reinterpret_cast<void**>(&b.set_cursor_pos)},{reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"ClipCursor")),reinterpret_cast<void*>(on_clip_cursor),reinterpret_cast<void**>(&b.clip_cursor)}};
  for(auto&hook:hooks){if(!ok)break;ok=MH_CreateHook(hook.target,hook.detour,hook.original)==MH_OK;if(ok)b.targets.push_back(hook.target);}
  if(ok)for(auto*t:b.targets)if(MH_EnableHook(t)!=MH_OK){ok=false;break;}
  if(!ok){for(auto*t:b.targets){MH_DisableHook(t);MH_RemoveHook(t);}b.targets.clear();}
 }DestroyWindow(dummy);
 if(ok){install_dinput_hooks();b.client=std::thread([&b]{b.ipc_worker();});b.log("DX12_HOOKS_INSTALLED; waiting for native swapchain creation (restart required for late injection)");}else b.log("DX12_HOOK_INSTALL_FAILED; native replay remains independent");return ok?1:0;
}
extern "C" void tm_render_shutdown(){camera_runtime::stop();auto&b=backend();b.running=false;if(b.client.joinable()){CancelSynchronousIo(b.client.native_handle());b.client.join();}
 for(auto*t:b.targets)MH_DisableHook(t);std::lock_guard lock(b.graphics);if(b.previous_proc&&IsWindow(b.hwnd)&&reinterpret_cast<WNDPROC>(GetWindowLongPtrW(b.hwnd,GWLP_WNDPROC))==TheaterRenderBackend::wndproc)SetWindowLongPtrW(b.hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(b.previous_proc));
 if(b.wait_gpu())b.release_resources();for(auto*t:b.targets)MH_RemoveHook(t);b.targets.clear();b.chain.Reset();b.queue.Reset();
 if(b.exception_observer){RemoveVectoredExceptionHandler(b.exception_observer);b.exception_observer=nullptr;}
}
// CPU-only UI construction test. Does not install hooks or assert game rendering.
extern "C" int tm_render_test_ui(){
 SetEnvironmentVariableW(L"THEATER_OVERLAY_NO_SETTINGS_FILE",L"1");
 auto&b=backend();auto*c=ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.DeltaTime=1.f/60;
 // No renderer backend here: let the atlas build on the CPU as the legacy path does.
 b.overlay.Init(io);io.IniFilename=nullptr; // never touch the user's saved layout from a test
 unsigned char*p=nullptr;int w=0,h=0;io.Fonts->GetTexDataAsRGBA32(&p,&w,&h);
 {std::lock_guard lock(b.ipc);b.snapshot={};b.snapshot.loaded=1;b.snapshot.connected=1;b.snapshot.player_found=1;b.snapshot.duration_ns=134'500'000'000;b.snapshot.time_ns=26'300'000'000;
  b.snapshot.count=16;b.snapshot.total=40;b.snapshot.phase=2;b.snapshot.recording_state=theater_ui::record_recording;b.snapshot.recording_ns=83'400'000'000;b.snapshot.recording_samples=2502;
  for(unsigned i=0;i<16;++i)b.snapshot.actors[i].id=i+1;
  b.snapshot.replay_total=30;b.snapshot.replay_count=theater_ui::replay_page_size;for(unsigned i=0;i<theater_ui::replay_page_size;++i){auto&e=b.snapshot.replays[i];snprintf(e.name,sizeof(e.name),"Replay_%03u_Лейнделл",i);e.index=i;e.duration_ns=(i+1)*9000000000ull;e.bytes=(i+1)*1048576ull;e.loaded=i==0;}}
 b.host_linked=true;
 bool valid=true;
 for(auto lang:{TheaterUI::Lang::English,TheaterUI::Lang::Russian}){b.overlay.language=lang;
  for(auto size:{ImVec2{1280,720},ImVec2{1920,1080},ImVec2{2560,1440},ImVec2{2560,1100},ImVec2{3440,1440},ImVec2{3840,2160},ImVec2{7680,2160}})
   for(auto vis:{TheaterUI::UiVisibility::Shown,TheaterUI::UiVisibility::Hidden,TheaterUI::UiVisibility::HiddenClean})
    for(auto tool:{TheaterUI::Tool::Scene,TheaterUI::Tool::Camera,TheaterUI::Tool::Look,TheaterUI::Tool::Replays,TheaterUI::Tool::Export,TheaterUI::Tool::Debug,TheaterUI::Tool::Settings}){
     b.visibility=int(vis);b.mode=vis==TheaterUI::UiVisibility::Shown?2:0;b.overlay.State().activeTool=tool;io.DisplaySize=size;
     ImGui::NewFrame();b.draw();ImGui::Render();valid=valid&&ImGui::GetDrawData()->Valid;
    }}
 // Cycling cameras is independent of showing the main overlay.
 b.visibility=int(TheaterUI::UiVisibility::Hidden);b.mode=0;camera_runtime::mode(0);
 for(unsigned expected:{1u,2u,0u}){b.overlay.CameraHotkey(theater_hotkeys::Action::CycleCamera);valid=valid&&b.visibility==int(TheaterUI::UiVisibility::Hidden)&&camera_runtime::view(false).mode==expected;}
 const bool markers=b.overlay.DollyControlsVisible();
 const auto previousTool=b.overlay.State().activeTool;
 b.overlay.CameraHotkey(theater_hotkeys::Action::ToggleDollyControls);
 valid=valid&&b.overlay.DollyControlsVisible()!=markers&&b.visibility==int(TheaterUI::UiVisibility::Hidden)&&b.overlay.State().activeTool==previousTool;
 b.overlay.CameraHotkey(theater_hotkeys::Action::ToggleDollyControls);
 valid=valid&&b.overlay.DollyControlsVisible()==markers;
 // These are selection checks only: no game camera can arm in this test process.
 // Exercise short/narrow panels at different font/layout scales. Each child must
 // have its own scroll range and stay clipped above the separate event log.
 b.visibility=int(TheaterUI::UiVisibility::Shown);b.mode=2;
 for(auto size:{ImVec2(1280,720),ImVec2(1920,1080),ImVec2(3840,2160)})
 for(float scale:{.75f,1.f,1.5f})for(auto tool:{TheaterUI::Tool::Camera,TheaterUI::Tool::Settings}){
  io.DisplaySize=size;b.overlay.State().layout.uiScaleUser=scale;b.overlay.State().activeTool=tool;
  for(int pass=0;pass<3;++pass){
   ImGui::NewFrame();
   if(auto*panel=ImGui::FindWindowByName("###panel")){
    ImGui::SetWindowPos(panel,ImVec2(30,50),ImGuiCond_Always);
    ImGui::SetWindowSize(panel,ImVec2(300*scale,360*scale),ImGuiCond_Always);
   }
   b.draw();ImGui::Render();valid=valid&&ImGui::GetDrawData()->Valid;
  }
  ImGuiWindow*content=nullptr;ImGuiWindow*log=nullptr;
  for(auto*window:ImGui::GetCurrentContext()->Windows)if(window->Active){
   if(strstr(window->Name,"##tool-content"))content=window;
   if(strstr(window->Name,"##log"))log=window;
  }
  valid=valid&&content&&log;
  if(content&&log){
   valid=valid&&content->ScrollMax.y>0&&!(content->Flags&ImGuiWindowFlags_NoScrollWithMouse)
    &&content->Pos.y+content->Size.y<=log->Pos.y+1
    &&log->Pos.y+log->Size.y<=log->ParentWindow->InnerRect.Max.y+1;
   if(!valid)fprintf(stderr,"Scrollable panel geometry failed: tool=%u scale=%.2f size=%.0fx%.0f scroll=%.2f content_bottom=%.2f log=%.2f..%.2f parent_bottom=%.2f\n",unsigned(tool),scale,size.x,size.y,content->ScrollMax.y,content->Pos.y+content->Size.y,log->Pos.y,log->Pos.y+log->Size.y,log->ParentWindow->InnerRect.Max.y);
  }
 }
 // Synthetic texture ID exercises CPU layout only; never submitted to a GPU.
 for(auto size:{ImVec2(1280,720),ImVec2(1920,1080),ImVec2(3840,2160)}){
  io.DisplaySize=size;b.visibility=int(TheaterUI::UiVisibility::Shown);b.mode=2;
  for(int pass=0;pass<4;++pass){ImGui::NewFrame();
   if(auto*seq=ImGui::FindWindowByName("###timeline")){ImGui::SetWindowPos(seq,ImVec2(50,size.y*.6f));ImGui::SetWindowSize(seq,ImVec2(size.x-70,size.y*.4f));}
   const auto rect=b.draw(ImTextureID(1));ImGui::Render();auto*seq=ImGui::FindWindowByName("###timeline");
   auto*bar=ImGui::FindWindowByName("##camera-modes");
   valid=valid&&bar&&rect.gameMin.y>=bar->Pos.y+bar->Size.y&&ImGui::GetDrawData()->Valid&&seq&&rect.gameMax.y<=seq->Pos.y&&rect.gameMin.x>=0&&rect.gameMax.x<=size.x&&rect.gameMax.y>rect.gameMin.y;
  }
 }
 const auto visibilityBefore=b.visibility.load();const auto actionsBefore=camera_actions.size(),commandsBefore=b.commands.size();
 for(auto key:{theater_hotkeys::Action::ToggleOverlay,theater_hotkeys::Action::TogglePlayback,theater_hotkeys::Action::ToggleAllHud,theater_hotkeys::Action::CycleCamera})TheaterRenderBackend::wndproc(nullptr,WM_KEYDOWN,theater_hotkeys::Key(key),0);
 valid=valid&&b.visibility==visibilityBefore&&camera_actions.size()==actionsBefore&&b.commands.size()==commandsBefore;
 b.host_linked=false;b.visibility=int(TheaterUI::UiVisibility::Hidden);b.mode=0;ImGui::DestroyContext(c);return valid?1:0;
}
// The hotkey table for the Rust side (shared/TheaterHotkeys.h). Unknown action: 0 (unbound).
// Bone replay link for adapter/src/bone_replay.rs: the host's recording and timeline state.
struct TmBoneLink{std::uint32_t linked,recording,loaded,playing,apply_requested,options;double timescale;std::uint64_t play_source_ns,received_ns;char recording_path[260];char loaded_path[260];};
static_assert(sizeof(TmBoneLink)==24+8+16+520);
extern "C" void tm_overlay_bone_link(TmBoneLink*out){auto&b=backend();*out={};out->linked=b.host_linked.load();out->options=TheaterUI::gReplayOptions.load();
 std::lock_guard lock(b.ipc);const auto&s=b.snapshot;out->recording=s.recording_state;out->loaded=s.loaded;out->playing=s.host_playing;out->timescale=s.timescale;out->apply_requested=s.application_requested;
 out->play_source_ns=s.play_source_ns;out->received_ns=s.master_clock_ns;memcpy(out->recording_path,s.recording_path,sizeof(out->recording_path));memcpy(out->loaded_path,s.loaded_path,sizeof(out->loaded_path));
 out->recording_path[259]=0;out->loaded_path[259]=0;}
extern "C" void tm_render_lock_game_input(int locked){backend().game_input_locked=locked!=0;}
extern "C" unsigned tm_hotkey_vk(unsigned action){return action<unsigned(theater_hotkeys::Action::Count)?theater_hotkeys::Key(theater_hotkeys::Action(action)):0u;}
extern "C" void tm_camera_publish(const theater_camera::Telemetry* snapshot){
 if(!snapshot)return;
 std::unique_lock lock(camera_mutex,std::try_to_lock);if(!lock.owns_lock())return;
 camera_snapshot=*snapshot;
 for(auto&slot:camera_snapshot.slots)slot.valid=slot.valid&&theater_camera::valid(slot);
}
extern "C" bool tm_camera_probe_enabled(){return theater_camera::probe_enabled.load();}

// Standalone GPU smoke fixture only, linked into the test EXE, not a Rust export.
extern "C" void tm_render_smoke_visibility(int shown){backend().set_visibility(shown?TheaterUI::UiVisibility::Shown:TheaterUI::UiVisibility::Hidden);}
