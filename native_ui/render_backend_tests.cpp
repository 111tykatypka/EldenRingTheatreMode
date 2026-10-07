#include "TheaterUiProtocol.h"
#include "TheaterLayout.h"
#include <cmath>
#include <iostream>
extern "C" int tm_render_test_ui();
// Rows from the v3 blueprint table (THEATER_UI_BLUEPRINT.md, section D): display, picture aspect,
// expected UiScale, game picture size, panel, selection column and sequencer heights in display px.
// The table's "21:9" rows are an ultrawide-fixed picture, i.e. the display's own aspect.
struct LayoutCase{float w,h,aspect,scale,gameW,gameH,panel,selection,sequencer;};
static bool near(float a,float b){return std::fabs(a-b)<=1.5f;}
int main(){using namespace theater_ui;Request r;r.sequence=1;
 if(!valid(r,0)||valid(r,1))return 1;
 for(auto kind:{play,pause,stop,restart,previous,next,record_start,record_stop}){r.command=kind;if(!valid(r,0))return 2;}
 r.command=record_start;r.value=1;if(valid(r,0))return 8;r.value=0;
 r.command=command_count;if(valid(r,0))return 9;
 r.command=replay_open;r.value=7;if(!valid(r,0))return 13;r.command=replay_page;r.value=12;if(!valid(r,0))return 14;r.value=0;
 r.command=timescale;for(auto value:{.01,.0105,.037,1.,4.}){r.value=theater_timescale::encode(value);if(!valid(r,0))return 3;}
 r.value=theater_timescale::encode(0.);if(valid(r,0))return 4;r.value=theater_timescale::encode(1.);r.reserved=1;if(valid(r,0))return 5;
 r.reserved=0;r.version=1;if(valid(r,0))return 10;r.version=version;
 r.magic_value=0;if(valid(r,0))return 6;
 const LayoutCase cases[]{
  {2560,1440,16.f/9,1.00f,2098,1180,398,0,260},{1920,1080,16.f/9,0.80f,1550,872,319,0,208},{3840,2160,16.f/9,1.50f,3147,1770,597,0,390},
  {2560,1600,16.f/10,1.00f,2144,1340,352,0,260},{2560,1100,2560.f/1100,0.80f,2076,892,352,0,208},{2560,1100,16.f/9,0.80f,1586,892,352,288,208},
  {3440,1440,3440.f/1440,1.00f,2819,1180,440,0,260},{3440,1440,16.f/9,1.00f,2098,1180,440,360,260},{1280,720,16.f/9,0.80f,910,512,319,0,208}};
 for(const auto&c:cases){TheaterUI::PanelLayout pl;const auto rects=TheaterUI::SolveLayout(ImVec2(c.w,c.h),c.aspect,pl,TheaterUI::UiVisibility::Shown);
  const float panel=rects.panelMax.x-rects.panelMin.x,selection=rects.selectionMax.x-rects.selectionMin.x;
  if(!near(rects.uiScale,c.scale)||!near(rects.gameMax.x-rects.gameMin.x,c.gameW)||!near(rects.gameMax.y-rects.gameMin.y,c.gameH)||
     !near(panel,c.panel)||!near(rects.hasSelectionColumn?selection:0,c.selection)||!near(rects.sequencerMax.y-rects.sequencerMin.y,c.sequencer)){
   std::cerr<<"layout mismatch at "<<c.w<<"x"<<c.h<<" aspect "<<c.aspect<<" game "<<rects.gameMax.x-rects.gameMin.x<<"x"<<rects.gameMax.y-rects.gameMin.y<<" panel "<<panel<<" selection "<<selection<<"\n";return 11;}
  if(TheaterUI::TrackRowsThatFit(rects)<6)return 12;}
 if(!tm_render_test_ui())return 7;
 std::cout<<"Editor wire v3 validation + v3 layout table + overlay construction (EN/RU, 7 resolutions, F4 states, all tools) PASS. No GPU/game verification.\n";
}
