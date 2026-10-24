#pragma once
#include <cmath>
// BONE_CONTAINMENT_V34: test the projected artwork's complete bounds against
// the current camera ROI using the native calibration at the candidate depth.
// Artwork bounds include its dark outline. Gesture recognition is untouched.
namespace MagicSandBoneRoi {
template<class Projector,class Rect,class Candidate>
bool contains(Projector& projector,const Rect& roi,const Candidate& c){
    if(!c.valid||!std::isfinite(c.depth)||c.depth<450||c.depth>4500)return false;
    // A visible center alone does not guarantee the icon fits inside the sensed
    // sand. Test its outline at a constant candidate depth; this is a local
    // planar approximation, not a guarantee against hand occlusion at height.
    const float hx=.43f*c.rx+2.f,hy=.25f*c.ry+2.f;
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){
        if(x==0&&y==0)continue;
        const float px=c.px+x*hx,py=c.py+y*hy;
        const auto world=projector.projCoordAndWorldZToWorldCoord(px,py,c.depth);
        if(!std::isfinite(world.x)||!std::isfinite(world.y)||!std::isfinite(world.z)||world.z<=0)return false;
        const auto camera=projector.worldCoordTokinectCoord(world);
        if(!std::isfinite(camera.x)||!std::isfinite(camera.y)||camera.x<roi.x+1.f||camera.y<roi.y+1.f||
            camera.x>roi.x+roi.width-1.f||camera.y>roi.y+roi.height-1.f)return false;
        // Reject singular/invalid inverses rather than accepting an arbitrary point.
        const auto roundTrip=projector.worldCoordToProjCoord(world);
        if(!std::isfinite(roundTrip.x)||!std::isfinite(roundTrip.y)||
            std::abs(roundTrip.x-px)>1.f||std::abs(roundTrip.y-py)>1.f)return false;
    }
    return true;
}
}
