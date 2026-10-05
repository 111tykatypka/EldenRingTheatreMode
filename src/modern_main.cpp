#include "modern_ui.hpp"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
namespace {
ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGISwapChain> swapchain;ComPtr<ID3D11RenderTargetView> target;
UINT width{},height{};bool done{};float dpi=1;
bool create_target(){ComPtr<ID3D11Texture2D> texture;return SUCCEEDED(swapchain->GetBuffer(0,IID_PPV_ARGS(&texture)))&&SUCCEEDED(device->CreateRenderTargetView(texture.Get(),nullptr,&target));}
bool create_device(HWND w){DXGI_SWAP_CHAIN_DESC d{};d.BufferCount=2;d.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.OutputWindow=w;d.SampleDesc.Count=1;d.Windowed=TRUE;d.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;D3D_FEATURE_LEVEL level{};const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_0};auto hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,levels,2,D3D11_SDK_VERSION,&d,&swapchain,&device,&level,&context);if(hr==DXGI_ERROR_UNSUPPORTED)hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,levels,2,D3D11_SDK_VERSION,&d,&swapchain,&device,&level,&context);return SUCCEEDED(hr)&&create_target();}
}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
LRESULT CALLBACK modern_proc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(m==WM_HOTKEY){if(a==2)theater::emergency_stop();else if(!ImGui::GetCurrentContext()||!ImGui::GetIO().WantCaptureKeyboard){if(a==1)theater::post_command(theater::Command::start);if(a==3)theater::post_command(theater::Command::pause);if(a==4)theater::post_command(theater::Command::resume);}return 0;}
 if(ImGui_ImplWin32_WndProcHandler(w,m,a,b))return 1;
 switch(m){case WM_SIZE:if(a!=SIZE_MINIMIZED){width=LOWORD(b);height=HIWORD(b);}return 0;
 case WM_DPICHANGED:{dpi=float(HIWORD(a))/96;auto*r=reinterpret_cast<RECT*>(b);SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
 case WM_ERASEBKGND:return 1;case WM_CLOSE:done=true;return 0;case WM_DESTROY:PostQuitMessage(0);return 0;case WM_SYSCOMMAND:if((a&0xfff0)==SC_KEYMENU)return 0;}
 return DefWindowProcW(w,m,a,b);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
 const HANDLE single=CreateMutexW(nullptr,FALSE,L"Local\\EldenRingTheaterMode_1_17_Host");if(!single)return 1;
 if(GetLastError()==ERROR_ALREADY_EXISTS){if(auto w=FindWindowW(L"EldenRingTheaterModeWindow",nullptr)){ShowWindow(w,SW_RESTORE);SetForegroundWindow(w);}CloseHandle(single);return 0;}
 ImGui_ImplWin32_EnableDpiAwareness();dpi=ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint({},MONITOR_DEFAULTTOPRIMARY));
 WNDCLASSW wc{};wc.lpfnWndProc=modern_proc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"EldenRingTheaterModeWindow";RegisterClassW(&wc);
 HWND window=CreateWindowW(wc.lpszClassName,L"Elden Ring Theater Mode | Modern",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,int(1440*dpi),int(900*dpi),nullptr,nullptr,instance,nullptr);
 if(!window||!create_device(window)){MessageBoxW(nullptr,L"DirectX 11 renderer initialization failed.",L"Theater Mode",MB_ICONERROR);CloseHandle(single);return 1;}
 IMGUI_CHECKVERSION();ImGui::CreateContext();auto&io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_DockingEnable|ImGuiConfigFlags_NavEnableKeyboard;
 ImGui::StyleColorsDark();auto base_style=ImGui::GetStyle();base_style.WindowRounding=3;base_style.FrameRounding=4;base_style.GrabRounding=4;base_style.Colors[ImGuiCol_WindowBg]={.075f,.09f,.12f,1};base_style.Colors[ImGuiCol_Header]={.16f,.29f,.34f,1};
 wchar_t windows_path[MAX_PATH]{};GetWindowsDirectoryW(windows_path,MAX_PATH);auto font=std::filesystem::path(windows_path)/L"Fonts"/L"segoeui.ttf";if(std::filesystem::exists(font))io.Fonts->AddFontFromFileTTF(game_launcher::utf8(font.wstring()).c_str(),17);else io.Fonts->AddFontDefault();
 ImGui_ImplWin32_Init(window);ImGui_ImplDX11_Init(device.Get(),context.Get());
 theater::initialize(window);editor::load_settings();const std::string ini=game_launcher::utf8((theater::app.root/L"Modern.layout.ini").wstring());io.IniFilename=ini.c_str();
 ShowWindow(window,SW_SHOWMAXIMIZED);float applied_dpi=0;bool failed=false;
 while(!done){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);if(msg.message==WM_QUIT)done=true;}if(done)break;
  if(IsIconic(window)){MsgWaitForMultipleObjects(0,nullptr,FALSE,40,QS_ALLINPUT);continue;}
  if(width&&height){target.Reset();auto hr=swapchain->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0);width=height=0;if(FAILED(hr)||!create_target()){failed=true;break;}}
  if(applied_dpi!=dpi){ImGui::GetStyle()=base_style;ImGui::GetStyle().ScaleAllSizes(dpi);ImGui::GetStyle().FontScaleDpi=dpi;applied_dpi=dpi;}
  // Copy worker state before the frame. No application lock is held by ImGui.
  const auto recorder=theater::recorder_view();const auto playback=theater::playback_view();const auto control=theater::app.control.state();const auto launcher=theater::app.launcher.state();
  ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();editor::draw(recorder,playback,control,launcher);ImGui::Render();
  const float clear[]{.055f,.067f,.085f,1};auto*view=target.Get();context->OMSetRenderTargets(1,&view,nullptr);context->ClearRenderTargetView(view,clear);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  auto hr=swapchain->Present(1,0);editor::dispatch();if(hr==DXGI_ERROR_DEVICE_REMOVED||hr==DXGI_ERROR_DEVICE_RESET){failed=true;break;}if(hr==DXGI_STATUS_OCCLUDED)MsgWaitForMultipleObjects(0,nullptr,FALSE,40,QS_ALLINPUT);
 }
 theater::shutdown();editor::save_settings();ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();target.Reset();swapchain.Reset();context.Reset();device.Reset();DestroyWindow(window);CloseHandle(single);
 if(failed)MessageBoxW(nullptr,L"Graphics device lost. Replay writes stopped. Restart the editor.",L"Theater Mode",MB_ICONERROR);return failed?1:0;
}
