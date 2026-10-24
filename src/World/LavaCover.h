/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
// AUTO_VOLCANO_V1. Cached physical lava/crust footprint, shared on main thread.
// No camera reads or per-animal terrain scans. Covers cooled crust as well.
namespace MagicSandLavaCover {
struct Map {
 float x=0,y=0,w=0,h=0;int cols=0,rows=0;
 std::vector<float> depth;
 void clear(){depth.clear();cols=rows=0;}
 void publish(float px,float py,float width,float height,int columns,int lines,
              const std::vector<float>& lava,const std::vector<float>& crust,
              const std::vector<unsigned char>& valid) {
  x=px;y=py;w=width;h=height;cols=columns;rows=lines;depth.resize(lava.size());
  for(size_t i=0;i<lava.size();++i)depth[i]=valid[i]?lava[i]+crust[i]:0;
 }
 bool covered(float px,float py)const {
  if(cols<1||rows<1||w<=0||h<=0||px<x||py<y||px>=x+w||py>=y+h)return false;
  float gx=(px-x)*cols/w-.5f,gy=(py-y)*rows/h-.5f;
  int ix=int(std::floor(gx)),iy=int(std::floor(gy));float tx=gx-ix,ty=gy-iy,sum=0;
  for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
   int xx=std::max(0,std::min(cols-1,ix+dx)),yy=std::max(0,std::min(rows-1,iy+dy));
   sum+=depth[yy*cols+xx]*(dx?tx:1-tx)*(dy?ty:1-ty);
  }
  return sum>.02f;
 }
 bool touches(float px,float py,float radius)const {
  if(covered(px,py))return true;
  for(int n=0;n<8;++n){float a=n*.785398163f;if(covered(px+radius*std::cos(a),py+radius*std::sin(a)))return true;}
  return false;
 }
};
inline Map& map(){static Map value;return value;}
}
