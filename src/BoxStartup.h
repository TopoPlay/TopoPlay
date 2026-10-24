/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ofMain.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
// BOX_SETTINGS_V7: one explicit, atomic startup snapshot of original controls.
namespace MagicSandBox {
inline std::string path(){return ofToDataPath("settings/box-startup.json",true);}
inline void validate(const ofJson& j){
 if(j.value("box_schema",0)!=1)throw std::runtime_error("Unknown box settings format");
 for(const char* key:{"camera_tilt_x","camera_tilt_y","camera_vertical_mm","camera_normal_x","camera_normal_y","camera_normal_z","camera_origin_x","camera_origin_y","camera_origin_z","camera_roi_x","camera_roi_y","camera_roi_w","camera_roi_h","camera_max_offset","camera_averaging","ceiling_mm","terrain_spacing_mm","animals_fish","animals_sharks","animals_rabbits"}){
    float n=j.value(key,NAN);if(!std::isfinite(n))throw std::runtime_error(std::string("Missing/invalid setting: ")+key);
 }
 for(const char* key:{"camera_depth_view","camera_color_view","camera_debug","camera_spatial","camera_inpaint","camera_full_frame","camera_quick","camera_show_roi","terrain_contours","terrain_editor","animals_mother_fish","animals_mother_rabbit","animals_flipped"})(void)j.value(key,false);
 (void)j.value("terrain_map_file",std::string());
 float x=j.value("camera_normal_x",0.f),y=j.value("camera_normal_y",0.f),z=j.value("camera_normal_z",0.f);
 if(x*x+y*y+z*z<.0001f||j.value("camera_roi_w",0.f)<=0||j.value("camera_roi_h",0.f)<=0)throw std::runtime_error("Invalid saved calibration geometry");
 int count=j.value("terrain_key_count",0);if(count<2||count>512)throw std::runtime_error("Invalid saved terrain colors");
 float previous=-INFINITY;
 for(int n=0;n<count;++n){std::string k="terrain_key_"+std::to_string(n);float h=j.value((k+"_mm").c_str(),NAN);
    if(!std::isfinite(h)||h<=previous)throw std::runtime_error("Invalid terrain height order");previous=h;
    for(const char* channel:{"_r","_g","_b","_a"}){int v=j.value((k+channel).c_str(),-1);if(v<0||v>255)throw std::runtime_error("Invalid terrain color");}
 }
}
inline bool load(ofJson& result){if(!ofFile::doesFileExist(path()))return false;auto candidate=ofLoadJson(path());validate(candidate);result=candidate;return true;}
inline void save(const ofJson& j){
 validate(j);const std::string target=path(),temp=target+".tmp",previous=target+".previous";
 if(!ofSavePrettyJson(temp,j))throw std::runtime_error("Unable to write box settings");
 if(ofFile::doesFileExist(target)){
    std::ifstream input(target,std::ios::binary);std::ofstream backup(previous,std::ios::binary|std::ios::trunc);
    if(!input||!backup){std::remove(temp.c_str());throw std::runtime_error("Unable to back up previous box settings");}
    backup<<input.rdbuf();backup.flush();if(!backup){std::remove(temp.c_str());throw std::runtime_error("Box settings backup failed");}
 }
 if(std::rename(temp.c_str(),target.c_str())!=0){std::remove(temp.c_str());throw std::runtime_error("Unable to finish saving box settings");}
}
}
