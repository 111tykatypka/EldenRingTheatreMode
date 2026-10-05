#pragma once
#include "game_control.hpp"
#include <shellapi.h>
#include <commctrl.h>
#include <algorithm>
#include <filesystem>

// Small development panel, separate from the existing replay layout.
class LocomotionTraceUI {
    game_control::Client& client_;
    HWND window_{},status_{},phases_{},start_{},stop_{},mark_{};
    HFONT font_{};
    bool hotkey_{};
    unsigned phase_{1};
    static constexpr wchar_t class_name[]=L"TheaterLocomotionTrace";
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
        auto*self=reinterpret_cast<LocomotionTraceUI*>(GetWindowLongPtrW(w,GWLP_USERDATA));
        if(m==WM_NCCREATE){self=static_cast<LocomotionTraceUI*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->window_=w;}
        if(!self)return DefWindowProcW(w,m,wp,lp);
        switch(m){
        case WM_CREATE:self->create();return 0;
        case WM_SIZE:self->layout();return 0;
        case WM_DPICHANGED:{const auto*r=reinterpret_cast<RECT*>(lp);SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);self->set_font();self->layout();return 0;}
        case WM_TIMER:self->update();return 0;
        case WM_HOTKEY:if(wp==90)self->next_marker();return 0;
        case WM_COMMAND:
            if(LOWORD(wp)==1){if(self->client_.trace(game_control::trace_start)){self->phase_=1;SendMessageW(self->phases_,CB_SETCURSEL,0,0);}}
            if(LOWORD(wp)==2)self->client_.trace(game_control::trace_stop);
            if(LOWORD(wp)==3){const auto i=SendMessageW(self->phases_,CB_GETCURSEL,0,0);if(i>=0&&i<6&&self->client_.trace(game_control::trace_mark,unsigned(i)+1))self->phase_=unsigned(i)+1;}
            if(LOWORD(wp)==4){const auto path=std::filesystem::temp_directory_path()/L"TheaterModeLocomotionTrace.log";ShellExecuteW(w,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
            return 0;
        case WM_CLOSE:self->client_.trace(game_control::trace_stop);DestroyWindow(w);return 0;
        case WM_DESTROY:KillTimer(w,1);if(self->hotkey_)UnregisterHotKey(w,90);if(self->font_)DeleteObject(self->font_);self->font_=nullptr;self->window_=nullptr;self->hotkey_=false;return 0;
        }
        return DefWindowProcW(w,m,wp,lp);
    }
    void create(){
        auto button=[&](const wchar_t*text,int id){return CreateWindowExW(0,L"BUTTON",text,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,0,0,0,window_,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);};
        start_=button(L"START STATE TRACE",1);stop_=button(L"STOP TRACE",2);mark_=button(L"MARK PHASE",3);button(L"OPEN TRACE LOG",4);
        phases_=CreateWindowExW(0,WC_COMBOBOXW,L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,0,0,0,0,window_,nullptr,GetModuleHandleW(nullptr),nullptr);
        for(auto label:{L"IDLE",L"WALK",L"RUN",L"SPRINT",L"ROLL",L"JUMP"})SendMessageW(phases_,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));SendMessageW(phases_,CB_SETCURSEL,0,0);
        status_=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,window_,nullptr,GetModuleHandleW(nullptr),nullptr);
        hotkey_=RegisterHotKey(window_,90,MOD_NOREPEAT,VK_F9)!=FALSE;
        set_font();layout();SetTimer(window_,1,150,nullptr);update();
    }
    void set_font(){if(font_)DeleteObject(font_);font_=CreateFontW(-MulDiv(10,GetDpiForWindow(window_),72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");EnumChildWindows(window_,[](HWND h,LPARAM f)->BOOL{SendMessageW(h,WM_SETFONT,static_cast<WPARAM>(f),TRUE);return TRUE;},reinterpret_cast<LPARAM>(font_));}
    void layout(){RECT r{};GetClientRect(window_,&r);int d=GetDpiForWindow(window_),m=MulDiv(12,d,96),g=MulDiv(8,d,96),h=MulDiv(34,d,96),width=std::max(1,int(r.right)-2*m),half=std::max(1,(width-g)/2);auto place=[](HWND w,int x,int y,int width,int h){SetWindowPos(w,nullptr,x,y,width,h,SWP_NOZORDER|SWP_NOACTIVATE);};place(start_,m,m,half,h);place(stop_,m+half+g,m,half,h);place(phases_,m,m+h+g,half,MulDiv(230,d,96));place(mark_,m+half+g,m+h+g,half,h);place(GetDlgItem(window_,4),m,m+2*(h+g),width,h);place(status_,m,m+3*(h+g),width,std::max(1,int(r.bottom)-m-3*(h+g)-m));}
    void update(){const auto s=client_.state();EnableWindow(start_,s.trace_supported&&s.ready&&!s.trace_active&&!s.pending&&s.replay_phase!=game_control::playing&&s.replay_phase!=game_control::paused);EnableWindow(stop_,s.trace_active||s.trace_requested);EnableWindow(mark_,s.trace_active&&!s.pending);static const wchar_t*names[]={L"UNMARKED",L"IDLE",L"WALK",L"RUN",L"SPRINT",L"ROLL",L"JUMP"};std::wstring text=s.trace_failed?L"TRACE ERROR: check TheaterModeGame.log":s.trace_active?L"TRACE ACTIVE":s.trace_requested?L"TRACE START PENDING":s.trace_supported?L"TRACE OFF":L"Trace unavailable: use matching Phase5C DLL";text+=L"\r\nMarker: "+std::wstring(names[phase_])+L" (user label, not detected gait)\r\n";text+=hotkey_?L"F9: next marker while the game has focus.\r\n":L"F9 unavailable; use MARK PHASE.\r\n";text+=L"F6: emergency Stop, including trace.\r\nSequence: Idle / Walk / Run / Sprint / Roll / Jump / Idle.\r\nHold each movement 3 seconds. Trace reads state only.\r\nNo WALK/RUN/SPRINT replay driver is verified yet.\r\nLog: %TEMP%\\TheaterModeLocomotionTrace.log";int n=GetWindowTextLengthW(status_);std::wstring old(n+1,L'\0');GetWindowTextW(status_,old.data(),n+1);old.resize(n);if(old!=text)SetWindowTextW(status_,text.c_str());}
    void next_marker(){if(!client_.state().trace_active)return;const auto next=phase_>=6?1:phase_+1;if(client_.trace(game_control::trace_mark,next)){phase_=next;SendMessageW(phases_,CB_SETCURSEL,next-1,0);Beep(650+next*80,50);}}
public:
    explicit LocomotionTraceUI(game_control::Client&client):client_(client){}
    void show(HWND parent){if(window_){ShowWindow(window_,SW_SHOWNORMAL);SetForegroundWindow(window_);return;}WNDCLASSW c{};c.lpfnWndProc=proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=class_name;c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);RegisterClassW(&c);CreateWindowExW(0,class_name,L"Phase5C locomotion state trace",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,680,450,parent,nullptr,c.hInstance,this);ShowWindow(window_,SW_SHOWNORMAL);}
    void close(){if(window_)SendMessageW(window_,WM_CLOSE,0,0);}
};
