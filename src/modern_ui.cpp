#include "modern_ui.hpp"
#include "editor_math.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <functional>
namespace editor {
using namespace theater;
namespace {
std::vector<std::function<void()>> commands;
fs::path selected,last_document;
std::uint64_t chosen_bookmark=UINT64_MAX;
TimeView timeline; bool reset_layout=false,show_diagnostics=true,show_settings=true;
double warning_threshold=15;std::string error;bool pending_restart=false;
float viewport_zoom=1;ImVec2 viewport_pan{};
std::string text(const std::wstring& s){return game_launcher::utf8(s);}
void label(const std::wstring&s){ImGui::TextUnformatted(text(s).c_str());}
void button(const char* caption,std::function<void()> fn){if(ImGui::Button(caption))commands.push_back(std::move(fn));}
void reveal(const fs::path& p){ShellExecuteW(nullptr,L"open",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
void choose_replay(){wchar_t path[32768]{};OPENFILENAMEW o{sizeof(o)};o.hwndOwner=app.window;o.lpstrFilter=L"ERPLAY\0*.erplay\0";o.lpstrFile=path;o.nMaxFile=32768;o.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;if(GetOpenFileNameW(&o)){selected=fs::absolute(path);open_replay(selected);}}
void request_play(const PlaybackView& p,const game_control::State& remote,bool restart){
 pending_restart=restart;
 if(p.loaded&&app.game_pid.load()&&!p.active&&!app.preview_path.empty()){
  const auto& first=app.preview_path.front();float pos[]{first.x,first.y,first.z};
  if(start_distance(pos,remote.live.position.data())>warning_threshold){ImGui::OpenPopup("Replay start location");return;}
 }
 commands.push_back(restart?restart_replay:play_replay);
}
void dock_layout(){
 auto id=ImGui::DockSpaceOverViewport(0,ImGui::GetMainViewport());
 if(reset_layout||!ImGui::DockBuilderGetNode(id)->IsSplitNode()){
  // Preserve user docking after the first complete layout.
  static bool initialized=false;if(initialized&&!reset_layout)return;initialized=true;reset_layout=false;
  ImGui::DockBuilderRemoveNode(id);ImGui::DockBuilderAddNode(id,ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(id,ImGui::GetMainViewport()->WorkSize);
  auto center=id;auto left=ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,.23f,nullptr,&center);
  auto right=ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,.29f,nullptr,&center);
  auto bottom=ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,.40f,nullptr,&center);
  auto left_bottom=ImGui::DockBuilderSplitNode(left,ImGuiDir_Down,.45f,nullptr,&left);
  ImGui::DockBuilderDockWindow("Replay Library",left);ImGui::DockBuilderDockWindow("Recorder",left_bottom);ImGui::DockBuilderDockWindow("Launcher",left_bottom);
  ImGui::DockBuilderDockWindow("Viewport",center);ImGui::DockBuilderDockWindow("Timeline",bottom);
  for(auto n:{"Inspector","Characters","Bookmarks","Diagnostics","Settings"})ImGui::DockBuilderDockWindow(n,right);
  ImGui::DockBuilderFinish(id);
 }
}
void draw_timeline(const PlaybackView& p){
 ImGui::Begin("Timeline");
 if(!p.loaded){ImGui::TextDisabled("Open a replay to inspect its tracks.");ImGui::End();return;}
 button("|<",restart_replay);ImGui::SameLine();button("< sample",[]{step_replay(-1);});ImGui::SameLine();button("sample >",[]{step_replay(1);});ImGui::SameLine();
 button("Stop / F6",emergency_stop);ImGui::SameLine();button("Pause",pause_replay);
 static int speed=3;const char* speeds[]{"0.1x","0.25x","0.5x","1.0x","2.0x","4.0x"};ImGui::SameLine();ImGui::SetNextItemWidth(80);
 if(ImGui::Combo("##speed",&speed,speeds,6)){const double value=std::array{.1,.25,.5,1.,2.,4.}[speed];commands.push_back([value]{std::lock_guard lock(app.replay_mutex);if(app.replay_player)app.replay_player->set_speed(value);});}
 const double duration=p.summary.duration_ns/1e9;
 if(ImGui::Button("Fit"))timeline.fit(duration);ImGui::SameLine();button("Beginning",[]{seek_replay(0);});ImGui::SameLine();button("End",[t=p.summary.duration_ns]{seek_replay(t);});ImGui::SameLine();button("+ Bookmark",bookmark_add);
 label(time_text(p.state.timestamp_ns)+L" / "+time_text(p.summary.duration_ns));
 static double jump=0;ImGui::SetNextItemWidth(100);ImGui::InputDouble("seconds",&jump,0,0,"%.3f");ImGui::SameLine();button("Seek",[duration]{seek_replay(static_cast<std::uint64_t>(std::clamp(jump,0.0,duration)*1e9));});
 ImGui::TextDisabled("Wheel: zoom | Middle drag: pan | Drag ruler: seek (native writes stop)");
 auto origin=ImGui::GetCursorScreenPos();auto size=ImGui::GetContentRegionAvail();size.x=std::max(size.x,1.f);size.y=std::max(size.y,120.f);
 ImGui::InvisibleButton("timeline_canvas",size,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonMiddle);
 auto& io=ImGui::GetIO();const bool hover=ImGui::IsItemHovered();const double local=io.MousePos.x-origin.x;
 if(hover&&io.MouseWheel!=0)timeline.zoom(std::pow(.8,io.MouseWheel),std::clamp(local/size.x,0.0,1.0),duration);
 if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Middle))timeline.pan(-io.MouseDelta.x/size.x*timeline.span,duration);
 if(ImGui::IsItemActive()&&ImGui::IsMouseDown(ImGuiMouseButton_Left)){auto t=static_cast<std::uint64_t>(timeline.time(local,size.x,duration)*1e9);commands.push_back([t]{seek_replay(t);});}
 auto* d=ImGui::GetWindowDrawList();d->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);d->AddRectFilled(origin,{origin.x+size.x,origin.y+size.y},IM_COL32(19,24,32,255));
 const double step=std::pow(10.0,std::floor(std::log10(std::max(.001,timeline.span/8))));
 for(double t=std::ceil(timeline.begin/step)*step;t<=timeline.begin+timeline.span;t+=step){float x=origin.x+float(timeline.pixel(t,size.x));d->AddLine({x,origin.y+22},{x,origin.y+size.y},IM_COL32(48,57,69,255));char b[40];snprintf(b,sizeof(b),"%.2fs",t);d->AddText({x+3,origin.y+3},IM_COL32(170,181,197,255),b);}
 d->AddRectFilled({origin.x,origin.y+38},{origin.x+size.x,origin.y+64},IM_COL32(40,83,100,255));d->AddText({origin.x+8,origin.y+42},IM_COL32(220,240,245,255),"Player / Transform");
 d->AddText({origin.x+8,origin.y+76},IM_COL32(177,185,197,255),p.summary.action_event_count?"Animation / raw observations":"Animation / unavailable in this recording");
 for(auto t:app.replay_bookmarks){float x=origin.x+float(timeline.pixel(t/1e9,size.x));d->AddTriangleFilled({x,origin.y+24},{x-5,origin.y+32},{x+5,origin.y+32},IM_COL32(247,194,84,255));}
 float x=origin.x+float(timeline.pixel(p.state.timestamp_ns/1e9,size.x));d->AddLine({x,origin.y},{x,origin.y+size.y},IM_COL32(100,219,212,255),2);
 d->PopClipRect();if(hover)ImGui::SetTooltip("%.3f s",timeline.time(local,size.x,duration));ImGui::End();
}
void viewport(const PlaybackView& p){
 ImGui::Begin("Viewport");if(ImGui::Button("Fit trajectory")){viewport_zoom=1;viewport_pan={};}ImGui::SameLine();ImGui::TextDisabled("X/Z data view | wheel zoom, middle drag");
 auto o=ImGui::GetCursorScreenPos();auto sz=ImGui::GetContentRegionAvail();sz.x=std::max(1.f,sz.x);sz.y=std::max(1.f,sz.y);ImGui::InvisibleButton("trajectory",sz,ImGuiButtonFlags_MouseButtonMiddle);
 if(ImGui::IsItemHovered())viewport_zoom=std::clamp(viewport_zoom*std::pow(1.1f,ImGui::GetIO().MouseWheel),.05f,100.f);
 if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Middle)){viewport_pan.x+=ImGui::GetIO().MouseDelta.x;viewport_pan.y+=ImGui::GetIO().MouseDelta.y;}
 auto*d=ImGui::GetWindowDrawList();d->PushClipRect(o,{o.x+sz.x,o.y+sz.y},true);d->AddRectFilled(o,{o.x+sz.x,o.y+sz.y},IM_COL32(17,21,28,255));
 if(p.loaded&&!app.preview_path.empty()){
  float minx=app.preview_path[0].x,maxx=minx,minz=app.preview_path[0].z,maxz=minz;for(auto v:app.preview_path){minx=std::min(minx,v.x);maxx=std::max(maxx,v.x);minz=std::min(minz,v.z);maxz=std::max(maxz,v.z);}
  float scale=std::max(1.f,std::min(sz.x-40,sz.y-40))/std::max({maxx-minx,maxz-minz,1.f})*viewport_zoom;
  auto project=[&](erplay::Vec3 v){return ImVec2{o.x+sz.x/2+(v.x-(minx+maxx)/2)*scale+viewport_pan.x,o.y+sz.y/2-(v.z-(minz+maxz)/2)*scale+viewport_pan.y};};
  for(size_t i=1;i<app.preview_path.size();++i)d->AddLine(project(app.preview_path[i-1]),project(app.preview_path[i]),IM_COL32(69,125,142,255),1.5f);
  auto start=project(app.preview_path.front()),end=project(app.preview_path.back()),current=project(p.state.position);d->AddCircleFilled(start,4,IM_COL32(128,207,130,255));d->AddText(start,IM_COL32(200,220,200,255),"START");d->AddCircle(end,5,IM_COL32(244,187,96,255));d->AddCircleFilled(current,5,IM_COL32(106,224,215,255));d->AddCircle(current,8,IM_COL32_WHITE);d->AddText({current.x+12,current.y},IM_COL32_WHITE,"Player");
 }d->PopClipRect();ImGui::End();
}
}
void load_settings(){std::ifstream f(app.root/L"Modern.settings");double value=15;if(f>>value&&std::isfinite(value)&&value>=0)warning_threshold=value;}
void save_settings(){std::ofstream f(app.root/L"Modern.settings");f<<warning_threshold;}
void dispatch(){auto pending=std::move(commands);commands.clear();for(auto& fn:pending)try{fn();}catch(const std::exception&e){error=e.what();log_line("EDITOR_ERROR="+error);}}
void draw(const Snapshot& recorder,const PlaybackView& p,const game_control::State& remote,const game_launcher::State& launch){
 if(last_document!=app.opened_replay){last_document=app.opened_replay;timeline.fit(p.summary.duration_ns/1e9);viewport_zoom=1;viewport_pan={};chosen_bookmark=UINT64_MAX;}
 if(ImGui::BeginMainMenuBar()){
  if(ImGui::BeginMenu("File")){if(ImGui::MenuItem("Open replay..."))commands.push_back(choose_replay);if(ImGui::MenuItem("Unload"))commands.push_back(unload_replay);if(ImGui::MenuItem("Replay folder"))commands.push_back([]{reveal(app.replays);});ImGui::EndMenu();}
  if(ImGui::BeginMenu("View")){ImGui::MenuItem("Diagnostics",nullptr,&show_diagnostics);ImGui::MenuItem("Settings",nullptr,&show_settings);if(ImGui::MenuItem("Reset docking"))reset_layout=true;ImGui::EndMenu();}
  ImGui::Text("  GAME %s | IPC %s | PLAYER %s | 1.17 / 2.7.0.0",app.game_pid.load()?"RUNNING":"OFF",remote.connected?"CONNECTED":"OFF",recorder.player?"FOUND":"WAITING");ImGui::EndMainMenuBar();
 }dock_layout();
 ImGui::Begin("Replay Library");button("Open...",choose_replay);ImGui::SameLine();button("Refresh",refresh_library);ImGui::SameLine();button("Folder",[]{reveal(app.replays);});
 if(ImGui::BeginTable("replays",6,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollX|ImGuiTableFlags_ScrollY,{0,std::max(100.f,ImGui::GetContentRegionAvail().y-76)})){
  for(auto n:{"Replay","Duration","Samples","Actions","Format","Size MB"})ImGui::TableSetupColumn(n);ImGui::TableHeadersRow();
  for(auto& e:recorder.replays){auto path=e.path;auto id=text(path.wstring());ImGui::PushID(id.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();if(ImGui::Selectable(text(path.filename().wstring()).c_str(),selected==path,ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowDoubleClick)){selected=path;if(ImGui::IsMouseDoubleClicked(0))commands.push_back([path]{open_replay(path);});}if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\nGame %s",id.c_str(),e.summary.metadata.game_version.c_str());ImGui::TableNextColumn();label(time_text(e.summary.duration_ns));ImGui::TableNextColumn();ImGui::Text("%llu",e.summary.sample_count);ImGui::TableNextColumn();ImGui::Text("%llu",e.summary.action_event_count);ImGui::TableNextColumn();ImGui::Text("v%u",e.summary.metadata.format_version);ImGui::TableNextColumn();ImGui::Text("%.2f",double(e.bytes)/1048576);ImGui::PopID();}ImGui::EndTable();
 }
 ImGui::BeginDisabled(selected.empty());button("Open selected",[]{open_replay(selected);});ImGui::SameLine();button("Duplicate",[]{auto out=selected.parent_path()/(selected.stem().wstring()+L"_copy.erplay");for(unsigned i=2;fs::exists(out);++i)out=selected.parent_path()/(selected.stem().wstring()+L"_copy_"+std::to_wstring(i)+L".erplay");fs::copy_file(selected,out);refresh_library();});ImGui::SameLine();
 if(ImGui::Button("Rename"))ImGui::OpenPopup("Rename replay");ImGui::SameLine();if(ImGui::Button("Delete"))ImGui::OpenPopup("Delete replay");ImGui::EndDisabled();button("Unload",unload_replay);
 if(ImGui::BeginPopupModal("Rename replay",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){static char name[512]{};ImGui::InputText("New filename",name,sizeof(name));if(ImGui::Button("Rename")){auto source=selected;auto target=source.parent_path()/fs::path(game_launcher::wide(name));if(target.extension()!=L".erplay")target+=L".erplay";commands.push_back([source,target]{if(target.parent_path()!=source.parent_path()||fs::exists(target))throw std::runtime_error("Choose an unused filename in this folder");const bool loaded=app.opened_replay==source;if(loaded)unload_replay();fs::rename(source,target);auto a=source,b=target;a+=L".bookmarks";b+=L".bookmarks";if(fs::exists(a))fs::rename(a,b);selected=target;refresh_library();if(loaded)open_replay(target);});ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 if(ImGui::BeginPopupModal("Delete replay",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){label(selected.filename().wstring());ImGui::TextWrapped("Permanently delete this recording and its bookmarks?");if(ImGui::Button("Delete permanently")){auto path=selected;commands.push_back([path]{if(app.opened_replay==path)unload_replay();fs::remove(path);auto b=path;b+=L".bookmarks";fs::remove(b);selected.clear();refresh_library();});ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}ImGui::End();
 ImGui::Begin("Recorder");const char* states[]{"IDLE","RECORDING","PAUSED","SAVING","READY","ERROR"};ImGui::Text("%s",states[int(recorder.state)]);label(time_text(recorder.active_ns));ImGui::Text("Samples %llu | %.2f Hz\nData %.2f MB | Drops %llu",recorder.samples,recorder.rate,double(recorder.bytes)/1048576,recorder.dropped);
 ImGui::BeginDisabled(p.active||!recorder.player);button("Start / F5",[]{post_command(Command::start);});ImGui::EndDisabled();ImGui::SameLine();button("Stop / F6",emergency_stop);button("Pause / F7",[]{post_command(Command::pause);});ImGui::SameLine();button("Resume / F8",[]{post_command(Command::resume);});label(recorder.error);ImGui::End();
 ImGui::Begin("Launcher");label(launch.diagnostic);ImGui::BeginDisabled(launch.busy()||app.game_pid.load()!=0||!app.stop_hotkey||!app.sample_pipe_ready);button("Start Elden Ring",launch_game);ImGui::EndDisabled();button("Choose YAFSML...",choose_loader);label(app.loader_path.wstring());button("Launch logs",[]{reveal(app.root/L"launch");});ImGui::End();
 ImGui::Begin("Inspector");ImGui::TextUnformatted(p.loaded?text(app.opened_replay.filename().wstring()).c_str():"No replay loaded");ImGui::TextWrapped("%s",text(p.diagnostic).c_str());ImGui::BeginDisabled(!p.loaded);if(ImGui::Button(app.game_pid.load()?"Play / Resume in game":"Play / Resume"))request_play(p,remote,false);ImGui::SameLine();if(ImGui::Button("Restart"))request_play(p,remote,true);ImGui::EndDisabled();
 if(ImGui::BeginPopupModal("Replay start location",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextWrapped("Recorded start is farther than the warning threshold. Map compatibility is UNKNOWN. Explicit playback moves the player to the first sample.");if(ImGui::Button("Play anyway / move to start")){commands.push_back(pending_restart?restart_replay:play_replay);ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
 ImGui::Separator();ImGui::Text("Position: %.4f  %.4f  %.4f",p.state.position.x,p.state.position.y,p.state.position.z);ImGui::Text("Quaternion: %.4f  %.4f  %.4f  %.4f",p.state.orientation.x,p.state.orientation.y,p.state.orientation.z,p.state.orientation.w);ImGui::Text("Raw animation: %d | semantics UNKNOWN",p.state.current_action.animation_id);ImGui::Text("Game: %s | Samples: %llu",p.summary.metadata.game_version.c_str(),p.summary.sample_count);ImGui::End();
 draw_timeline(p);viewport(p);
 ImGui::Begin("Bookmarks");button("Add current time",bookmark_add);for(auto t:app.replay_bookmarks){ImGui::PushID(std::to_string(t).c_str());if(ImGui::Selectable(text(time_text(t)).c_str(),chosen_bookmark==t,ImGuiSelectableFlags_AllowDoubleClick)){chosen_bookmark=t;if(ImGui::IsMouseDoubleClicked(0))commands.push_back([t]{seek_replay(t);});}ImGui::PopID();}ImGui::BeginDisabled(chosen_bookmark==UINT64_MAX);button("Jump",[]{seek_replay(chosen_bookmark);});ImGui::SameLine();button("Remove",[]{bookmark_delete(chosen_bookmark);chosen_bookmark=UINT64_MAX;});ImGui::EndDisabled();ImGui::End();
 ImGui::Begin("Characters");ImGui::TextDisabled("Character tracks are being integrated.\nNo fabricated NPC data.");ImGui::End();
 if(show_settings){ImGui::Begin("Settings",&show_settings);ImGui::InputDouble("Start distance warning",&warning_threshold,1,10,"%.1f");warning_threshold=std::isfinite(warning_threshold)?std::max(0.,warning_threshold):15;button("Save settings",save_settings);ImGui::BeginDisabled(p.active);ImGui::Checkbox("Experimental raw animation requests",&app.animation);static int limit=0;if(ImGui::Combo("Native duration",&limit,"Full replay\0First 5 seconds\0First 10 seconds\0"))app.limit_ns=limit==1?5'000'000'000ULL:limit==2?10'000'000'000ULL:0;ImGui::EndDisabled();ImGui::TextWrapped("Input lock: FAILED AT RUNTIME in previous build. Grounding: unresolved. NPC native playback: not enabled.");ImGui::End();}
 if(show_diagnostics){ImGui::Begin("Diagnostics",&show_diagnostics);ImGui::Text("UI %.1f FPS | worker %.2f Hz | IPC %.2f Hz",ImGui::GetIO().Framerate,p.clock_hz,remote.replay_send_hz);ImGui::Text("Sample IPC latency %.3f ms | reconnects %llu",recorder.latency_ms,recorder.reconnects);label(remote.diagnostic);ImGui::TextWrapped("%s",error.c_str());button("Recorder logs",[]{reveal(app.logs);});ImGui::SameLine();button("Game log",[]{reveal(fs::temp_directory_path()/L"TheaterModeGame.log");});button("Trace start",[]{app.control.trace(game_control::trace_start);});ImGui::SameLine();button("Trace stop",[]{app.control.trace(game_control::trace_stop);});button("Probe +0.5 X once",[]{if(!playback_view().active)app.control.nudge();});ImGui::End();}
 if(!ImGui::GetIO().WantCaptureKeyboard){if(ImGui::IsKeyPressed(ImGuiKey_Space)){if(p.state.status==replay::Status::playing)commands.push_back(pause_replay);else request_play(p,remote,false);}if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow))commands.push_back([]{step_replay(-1);});if(ImGui::IsKeyPressed(ImGuiKey_RightArrow))commands.push_back([]{step_replay(1);});}
}
}
