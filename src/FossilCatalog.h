#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// TopoPlay addition: 2026-10-24. One stable image shared by buried-reference
// sampling and reveal drawing. New PNGs in dig/fossils join the next round.
#include "ofMain.h"
#include <algorithm>
#include <string>
#include <vector>
class TopoPlayFossilCatalog {
public:
    bool choose(){
        const std::string previous=path;
        std::vector<std::string> candidates;
        ofDirectory directory(ofToDataPath("dig/fossils",true));
        directory.allowExt("png");directory.listDir();
        for(std::size_t i=0;i<directory.size();++i)candidates.push_back(directory.getPath(i));
        candidates.push_back(ofToDataPath("dig/dinotrex.png",true));
        // Avoid an immediate repeat when another readable fossil is available.
        if(candidates.size()>1)candidates.erase(std::remove(candidates.begin(),candidates.end(),previous),candidates.end());
        while(!candidates.empty()){
            const auto index=std::min(candidates.size()-1,static_cast<std::size_t>(ofRandom(static_cast<float>(candidates.size()))));
            const auto candidate=candidates[index];candidates.erase(candidates.begin()+index);
            if(load(candidate))return true;
        }
        // Added files may be unreadable; retain the previous valid fossil then.
        if(!previous.empty() && load(previous))return true;
        path.clear();image.clear();return false;
    }
    ofImage& selectedImage(){return image;}
    const std::string& selectedPath()const{return path;}
private:
    ofImage image;
    std::string path;
    bool load(const std::string& candidate){
        if(!image.load(candidate) || !image.isAllocated() || image.getWidth()<=0 || image.getHeight()<=0)return false;
        path=candidate;return true;
    }
};
