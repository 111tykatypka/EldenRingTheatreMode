#include "TheaterUiProtocol.h"
#include <iostream>
extern "C" int tm_render_test_ui();
int main(){using namespace theater_ui;Request r;r.sequence=1;
 if(!valid(r,0)||valid(r,1))return 1;
 for(auto kind:{play,pause,stop,restart,previous,next}){r.command=kind;if(!valid(r,0))return 2;}
 r.command=speed;for(auto value:{10,25,50,100,200,400}){r.value=value;if(!valid(r,0))return 3;}
 r.value=123;if(valid(r,0))return 4;r.value=100;r.reserved=1;if(valid(r,0))return 5;
 r.reserved=0;r.magic_value=0;if(valid(r,0))return 6;
 if(!tm_render_test_ui())return 7;
 std::cout<<"Editor wire validation + ImGui Overlay/Editor construction at three resolutions PASS. No GPU/game verification.\n";
}
