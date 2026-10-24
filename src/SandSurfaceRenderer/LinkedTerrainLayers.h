#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include "RealisticTerrainLayers.h"
#include <algorithm>
namespace MagicSandLinkedLayers {
using Heights=MagicSandTerrainLayers::AllLevels;
using Pins=std::array<bool,20>;
inline Pins defaults(){Pins pins{};pins[7]=true;pins[16]=true;return pins;}
struct State{Pins pins=defaults();Heights basis{};int row=-1;float ceiling=0;};
struct Limits{int left=-1,right=20;float bottom=0,top=0,low=0,high=0;bool valid=false;};
constexpr float gap=1.0f; // Physical mm, independent of display units.
inline Limits limits(const Heights& base,const Pins& pins,int row,float ceiling){
    Limits l;
    if(row<0 || row>=20 || !std::isfinite(ceiling) || ceiling<=0 || !MagicSandTerrainLayers::finite(base))return l;
    for(int n=row-1;n>=0;--n)if(pins[n]){l.left=n;break;}
    for(int n=row+1;n<20;++n)if(pins[n]){l.right=n;break;}
    l.bottom=l.left>=0?base[l.left]:-ceiling;
    l.top=l.right<20?base[l.right]:ceiling;
    l.low=std::max(-ceiling,l.bottom+gap*(row-l.left));
    l.high=std::min(ceiling,l.top-gap*(l.right-row));
    l.valid=!pins[row] && l.high>l.low+0.00001f;
    return l;
}
// Scale original band widths proportionally. When a thin band reaches the
// minimum, hold it there and redistribute the rest among the remaining bands.
// The saved basis prevents drift and restores proportions after extreme moves.
inline void side(Heights& out,const Heights& base,int first,int last,
                 float oldLow,float oldHigh,float low,float high){
    const int count=last-first;
    float weights[21]={},widths[21]={},previous=oldLow;
    bool fixed[21]={};
    for(int k=1;k<=count;++k){
        float next=k==count?oldHigh:base[first+k];
        weights[k-1]=std::max(0.0f,next-previous);previous=next;
    }
    float remaining=high-low;
    for(int pass=0;pass<=count;++pass){
        float sum=0;int active=0;
        for(int k=0;k<count;++k)if(!fixed[k]){sum+=weights[k];++active;}
        if(!active)break;
        bool changed=false;
        for(int k=0;k<count;++k)if(!fixed[k]){
            float proposed=sum>0.00001f?remaining*weights[k]/sum:remaining/active;
            if(proposed<gap-0.00001f){fixed[k]=true;widths[k]=gap;changed=true;}
        }
        if(changed){remaining=high-low;for(int k=0;k<count;++k)if(fixed[k])remaining-=widths[k];}
        else{
            for(int k=0;k<count;++k)if(!fixed[k])widths[k]=sum>0.00001f?remaining*weights[k]/sum:remaining/active;
            break;
        }
    }
    float position=low;
    for(int k=1;k<count;++k){position+=widths[k-1];out[first+k]=position;}
}
inline bool move(const Heights& base,const Pins& pins,int row,float value,float ceiling,Heights& out){
    auto l=limits(base,pins,row,ceiling);
    if(!l.valid || !std::isfinite(value))return false;
    value=std::max(l.low,std::min(l.high,value));out=base;
    side(out,base,l.left,row,l.bottom,base[row],l.bottom,value);
    side(out,base,row,l.right,base[row],l.top,value,l.top);
    out[row]=value;return true;
}
}
