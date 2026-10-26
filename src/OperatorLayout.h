/* TopoPlay contributor credit — 2026-10-25
 * Todd Taylor and Grayson Taylor — TopoPlay development and testing.
 * This credit applies to TopoPlay contributions; original authors retain
 * credit for their work. Existing copyright and license notices are preserved.
 */
/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
// EDGE_GUI_V6: operator-window geometry only; projector output is unchanged.
namespace MagicSandOperator {
struct Rect{float x=0,y=0,w=0,h=0;Rect(){}Rect(float x,float y,float w,float h):x(x),y(y),w(w),h(h){}};
struct Heights{float terrain=0,projector=0,status=0,camera=0,triggers=0,animals=0,demo=0,cooling=0;};
struct Layout{Rect terrain,projector,status,camera,triggers,animals,demo,cooling,preview;float leftMax=0,rightMax=0;};
inline float columnWidth(float w){return std::max(190.0f,std::min(280.0f,w*.22f));}
inline Layout arrange(float w,float h,const Heights& a,float previewW,float previewH,float leftScroll=0,float rightScroll=0){
 const float gap=8,col=columnWidth(w),right=w-gap-col;
 Layout r;float y=gap;
 r.terrain={gap,y,col,a.terrain};if(a.terrain>0)y+=a.terrain+gap;
 r.projector={gap,y,col,a.projector};if(a.projector>0)y+=a.projector+gap;
 r.status={gap,std::max(y,h-gap-a.status),col,a.status};
 r.leftMax=std::max(0.0f,r.status.y+r.status.h+gap-h);
 float scroll=std::max(0.0f,std::min(leftScroll,r.leftMax));r.terrain.y-=scroll;r.projector.y-=scroll;r.status.y-=scroll;
 y=gap;r.camera={right,y,col,a.camera};if(a.camera>0)y+=a.camera+gap;
 r.triggers={right,y,col,a.triggers};if(a.triggers>0)y+=a.triggers+gap;
 r.animals={right,std::max(y,h-gap-a.animals),col,a.animals};
 r.rightMax=std::max(0.0f,r.animals.y+r.animals.h+gap-h);
 scroll=std::max(0.0f,std::min(rightScroll,r.rightMax));r.camera.y-=scroll;r.triggers.y-=scroll;r.animals.y-=scroll;
 const float centerX=col+gap*2,centerW=std::max(1.0f,w-2*col-4*gap);
 const float panelW=std::min(300.0f,(centerW-gap)*.5f);
 const float startX=centerX+(centerW-(panelW*2+gap))*.5f;
 const float bottomH=std::max(a.demo,a.cooling),bottomY=h-gap-bottomH;
 r.demo={startX,h-gap-a.demo,panelW,a.demo};r.cooling={startX+panelW+gap,h-gap-a.cooling,panelW,a.cooling};
 // Reserve a visible, fixed navigation strip when the left column overflows.
 const float previewTop=r.leftMax>0?48.0f:gap;
 const float regionH=std::max(1.0f,bottomY-gap-previewTop);
 const float scale=std::min(centerW/std::max(1.0f,previewW),regionH/std::max(1.0f,previewH));
 r.preview={centerX+(centerW-previewW*scale)*.5f,previewTop+(regionH-previewH*scale)*.5f,previewW*scale,previewH*scale};
 return r;
}
}
