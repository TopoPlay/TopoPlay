#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// TopoPlay experimental calibration rebuild, 2026-10-24.
// Normalized projective least squares; checkerboard symmetry is resolved from
// the ten board centers, then refined using every measured corner.
#include "libs/dlib/matrix.h"
#include "libs/dlib/matrix/matrix_qr.h"
#include <array>
#include <vector>
#include <cmath>
#include <string>
#include <limits>
#include <algorithm>
namespace TopoPlayFreshCalibration {
struct P3{double x=0,y=0,z=0;};
struct P2{double x=0,y=0;};
using Model=std::array<double,12>;
struct Result{
    bool ok=false;std::string message;
    double mean=0,maximum=0,depthSeparation=0;
    Model model{};std::vector<P3> ordered;
    std::array<int,10> symmetry{};
};
inline bool finite(P3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline bool finite(P2 p){return std::isfinite(p.x)&&std::isfinite(p.y);}
inline bool project(const Model& m,P3 p,P2& q){
    double d=m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11];
    if(!std::isfinite(d)||std::abs(d)<1e-8)return false;
    q={(m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3])/d,
       (m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7])/d};return finite(q);
}
inline bool fit(const std::vector<P3>& x,const std::vector<P2>& y,Model& model){
    if(x.size()!=y.size()||x.size()<6)return false;
    P3 xm;P2 ym;
    for(size_t i=0;i<x.size();++i){if(!finite(x[i])||!finite(y[i]))return false;xm.x+=x[i].x;xm.y+=x[i].y;xm.z+=x[i].z;ym.x+=y[i].x;ym.y+=y[i].y;}
    const double n=double(x.size());xm.x/=n;xm.y/=n;xm.z/=n;ym.x/=n;ym.y/=n;
    double xs=0,ys=0;
    for(size_t i=0;i<x.size();++i){xs+=std::pow(x[i].x-xm.x,2)+std::pow(x[i].y-xm.y,2)+std::pow(x[i].z-xm.z,2);ys+=std::pow(y[i].x-ym.x,2)+std::pow(y[i].y-ym.y,2);}
    xs=std::sqrt(xs/n);ys=std::sqrt(ys/n);if(xs<1e-6||ys<1e-6)return false;
    dlib::matrix<double,0,11> a(2*x.size(),11);dlib::matrix<double,0,1> b(2*x.size());a=0;
    for(size_t i=0;i<x.size();++i){
        const double p[4]={(x[i].x-xm.x)/xs,(x[i].y-xm.y)/xs,(x[i].z-xm.z)/xs,1};
        const double u=(y[i].x-ym.x)/ys,v=(y[i].y-ym.y)/ys;
        for(int k=0;k<4;++k){a(2*i,k)=p[k];a(2*i+1,k+4)=p[k];}
        for(int k=0;k<3;++k){a(2*i,k+8)=-u*p[k];a(2*i+1,k+8)=-v*p[k];}
        b(2*i)=u;b(2*i+1)=v;
    }
    dlib::qr_decomposition<dlib::matrix<double,0,11>> qr(a);
    const auto r=qr.get_r();double largest=0,smallest=std::numeric_limits<double>::infinity();
    for(int i=0;i<11;++i){double v=std::abs(r(i,i));largest=std::max(largest,v);smallest=std::min(smallest,v);}
    if(!std::isfinite(smallest)||smallest<largest*1e-9)return false;
    auto coefficients=dlib::matrix<double,11,1>(qr.solve(b));
    double normalized[3][4];for(int r=0;r<3;++r)for(int c=0;c<4;++c)normalized[r][c]=(r*4+c<11?coefficients(r*4+c):1);
    double intermediate[3][4];const double origin[3]={xm.x,xm.y,xm.z};
    for(int r=0;r<3;++r){
        for(int c=0;c<3;++c)intermediate[r][c]=normalized[r][c]/xs;
        intermediate[r][3]=normalized[r][3];for(int c=0;c<3;++c)intermediate[r][3]-=normalized[r][c]*origin[c]/xs;
    }
    for(int c=0;c<4;++c){model[c]=ys*intermediate[0][c]+ym.x*intermediate[2][c];model[4+c]=ys*intermediate[1][c]+ym.y*intermediate[2][c];model[8+c]=intermediate[2][c];}
    double divisor=model[11];if(!std::isfinite(divisor)||std::abs(divisor)<1e-12)return false;
    for(auto& value:model){value/=divisor;if(!std::isfinite(value))return false;}
    return true;
}
inline size_t corner(size_t batch,int index,int symmetry){
    int r=index/4,c=index%4;if(symmetry&1)c=3-c;if(symmetry&2)r=2-r;
    return batch*12+r*4+c;
}
inline Result solve(const std::vector<P3>& world,const std::vector<P2>& image,double width,double height){
    Result result;
    if(world.size()!=120||image.size()!=120){result.message="Expected exactly 120 fresh calibration points";return result;}
    if(width<=0||height<=0){result.message="Invalid projector resolution";return result;}
    std::vector<P3> centers(10);std::vector<P2> targets(10);
    for(size_t i=0;i<120;++i){
        if(!finite(world[i])||world[i].z<450||world[i].z>4500||!finite(image[i])||image[i].x<0||image[i].y<0||image[i].x>=width||image[i].y>=height){result.message="Invalid or clipped calibration measurement";return result;}
        auto& c=centers[i/12];auto& t=targets[i/12];c.x+=world[i].x/12;c.y+=world[i].y/12;c.z+=world[i].z/12;t.x+=image[i].x/12;t.y+=image[i].y/12;
    }
    // The first checkerboard is centered on each plane, allowing a comparison
    // without confusing plane tilt or different corner-board locations.
    result.depthSeparation=centers[0].z-centers[5].z;
    if(result.depthSeparation<30){result.message="Raised board not measured: need at least 30 mm depth separation";return result;}
    if(!fit(centers,targets,result.model)){result.message="Calibration centers are degenerate";return result;}
    result.ordered.resize(120);
    for(int iteration=0;iteration<3;++iteration){
        for(size_t batch=0;batch<10;++batch){
            double best=std::numeric_limits<double>::infinity();int choice=-1;
            for(int symmetry=0;symmetry<4;++symmetry){
                double error=0;
                for(int i=0;i<12;++i){P2 p;if(!project(result.model,world[corner(batch,i,symmetry)],p)){error=std::numeric_limits<double>::infinity();break;}
                    const auto& target=image[batch*12+i];error+=std::pow(p.x-target.x,2)+std::pow(p.y-target.y,2);}
                if(error<best){best=error;choice=symmetry;}
            }
            if(choice<0){result.message="Unable to resolve checkerboard orientation";return result;}
            result.symmetry[batch]=choice;
            for(int i=0;i<12;++i)result.ordered[batch*12+i]=world[corner(batch,i,choice)];
        }
        if(!fit(result.ordered,image,result.model)){result.message="Calibration corners are degenerate";return result;}
    }
    double sign=0;
    for(size_t i=0;i<120;++i){P2 p;if(!project(result.model,result.ordered[i],p)){result.message="Invalid projection denominator";return result;}
        const auto& x=result.ordered[i];double denominator=result.model[8]*x.x+result.model[9]*x.y+result.model[10]*x.z+1;
        if(i==0)sign=denominator;if(denominator*sign<=0){result.message="Projection crosses a singularity inside measurements";return result;}
        double e=std::hypot(p.x-image[i].x,p.y-image[i].y);result.mean+=e/120;result.maximum=std::max(result.maximum,e);
    }
    const double scale=std::max(width/1280.0,height/720.0);
    if(result.mean>5*scale||result.maximum>15*scale){result.message="Calibration rejected: reprojection error too large";return result;}
    result.ok=true;result.message="Calibration accepted";return result;
}
}
