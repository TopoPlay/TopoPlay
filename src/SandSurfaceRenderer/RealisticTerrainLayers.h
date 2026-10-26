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
#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
#include <array>
#include <cmath>
namespace MagicSandTerrainLayers {
using Levels=std::array<float,13>;
inline Levels defaults(){return {{0,4,5,15,51,65,80,96,134,165,175,190,210}};}
inline bool valid(const Levels& v){
    if(!std::isfinite(v[0]))return false;
    for(int i=1;i<13;++i)if(!std::isfinite(v[i]) || v[i]<v[i-1])return false;
    return true;
}
using AllLevels=std::array<float,20>;
inline bool finite(const AllLevels& values){
    for(float h:values)if(!std::isfinite(h))return false;
    return true;
}
// Desired positions are never overwritten by collision handling. Only the
// rendered positions collapse around the last manually edited boundary. Moving
// it back releases its neighbors to their own chosen positions, even after save.
inline AllLevels resolve(const AllLevels& desired,int anchor){
    AllLevels result=desired;
    if(anchor<0 || anchor>=20)anchor=0;
    for(int n=anchor-1;n>=0;--n)result[n]=std::fmin(result[n],result[n+1]);
    for(int n=anchor+1;n<20;++n)result[n]=std::fmax(result[n],result[n-1]);
    return result;
}
inline const char* name(int row){
    static const char* names[20]={"Deep water","Ocean blue","Mid water","Shallow water","Water edge","Shore blue","Wet shoreline","Water / sand boundary","Yellow sand begins","Yellow sand ends","Grass begins","Grass fades","Upper grass","Gravel begins","Rock patches","Mountain rock","Snow begins","Snow builds","Solid snow cap","Heavy snow"};
    return row>=0 && row<20?names[row]:"Terrain";
}
}
