/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <vector>
namespace MagicSandVisibleROI {
struct Rect {int x=0,y=0,w=0,h=0;};
// Largest axis-aligned rectangle whose entire pixel footprint is accepted.
// Histogram/stack scan: O(width*height), no camera/projector orientation assumption.
template<class Valid> Rect fit(Rect box,Valid valid){
    Rect best;int area=0;std::vector<int> heights(box.w,0),stack;
    for(int y=0;y<box.h;++y){
        for(int x=0;x<box.w;++x)heights[x]=valid(box.x+x,box.y+y)?heights[x]+1:0;
        stack.clear();
        for(int x=0;x<=box.w;++x){int h=x==box.w?0:heights[x];
            while(!stack.empty()&&heights[stack.back()]>h){
                int top=stack.back();stack.pop_back();int start=stack.empty()?0:stack.back()+1;
                int a=heights[top]*(x-start);
                if(a>area){area=a;best={box.x+start,box.y+y-heights[top]+1,x-start,heights[top]};}
            }
            if(x<box.w)stack.push_back(x);
        }
    }
    return best;
}
}
