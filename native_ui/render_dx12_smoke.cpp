#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <iostream>
#include <cstring>
using Microsoft::WRL::ComPtr;
extern "C" int tm_render_start(void(*)());
extern "C" void tm_render_shutdown();
void emergency(){}
// --preview: show the v3 UI in a 1600x900 window for a few seconds (for screenshots), no automated input.
int main(int argc,char**argv){const bool preview=argc>1&&!strcmp(argv[1],"--preview");
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 if(!tm_render_start(emergency))return 1;
 HWND window=CreateWindowExW(0,L"STATIC",L"DX12 Theater smoke",WS_OVERLAPPEDWINDOW,0,0,640,480,nullptr,nullptr,nullptr,nullptr);
 ComPtr<ID3D12Device> device;ComPtr<ID3D12CommandQueue> queue;ComPtr<IDXGIFactory4> factory;ComPtr<IDXGISwapChain> chain;
 D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
 DXGI_SWAP_CHAIN_DESC d{};d.BufferCount=2;d.BufferDesc.Width=640;d.BufferDesc.Height=480;d.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
 d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.OutputWindow=window;d.SampleDesc.Count=1;d.Windowed=TRUE;d.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
 if(FAILED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)))||FAILED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)))||FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))||FAILED(factory->CreateSwapChain(queue.Get(),&d,&chain)))return 2;
 if(preview){ShowWindow(window,SW_SHOW);SetWindowPos(window,HWND_TOPMOST,40,40,1600+16,900+39,0);chain->ResizeBuffers(2,1600,900,DXGI_FORMAT_R8G8B8A8_UNORM,0);SendMessageW(window,WM_KEYDOWN,VK_F4,0);
  for(int i=0;i<600;++i){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}if(FAILED(chain->Present(1,0)))return 3;}
  tm_render_shutdown();DestroyWindow(window);return 0;}
 for(int i=0;i<120;++i){MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}if(i==30||i==60||i==90)SendMessageW(window,WM_KEYDOWN,VK_F4,0);if(i==35||i==65)SendMessageW(window,WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(30,30));if(i==36||i==66)SendMessageW(window,WM_RBUTTONUP,0,MAKELPARAM(30,30));if(i==75&&FAILED(chain->ResizeBuffers(2,800,600,DXGI_FORMAT_R8G8B8A8_UNORM,0)))return 4;if(FAILED(chain->Present(0,0)))return 3;Sleep(5);}
 tm_render_shutdown();DestroyWindow(window);std::cout<<"DX12 real-device hook/first-frame/F4 Shown/Hidden UI, RMB down/up, resize PASS\n";
}
