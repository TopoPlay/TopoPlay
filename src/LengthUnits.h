/* TopoPlay modification notice — 2026-10-23
 * Added in the TopoPlay educational fork.
 * This update adds source-release licensing/provenance notices.
 * Earlier local development dates were not individually recorded.
 * Original notices below are retained; see docs/MODIFICATIONS.md.
 */
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "ofMain.h"
#include "ofxDatGui.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
namespace MagicSandUnits {
struct Entry {ofxDatGuiSlider* p;std::string name;float lo,hi,value;};
inline std::vector<Entry>& entries(){static std::vector<Entry> v;return v;}
inline bool& inches(){static bool v=false;return v;}
inline unsigned& revision(){static unsigned v=0;return v;}
inline void load(){static bool done=false;if(done)return;done=true;try{auto path=ofToDataPath("display-units.json",true);if(ofFile::doesFileExist(path))inches()=ofLoadJson(path).value("inches",false);}catch(const std::exception&) {}}
inline float display(float mm){load();return inches()?mm/25.4f:mm;}
inline std::string text(float mm){return ofToString(display(mm),inches()?2:0)+(inches()?" in":" mm");}
inline Entry* find(ofxDatGuiSlider* p){for(auto& e:entries())if(e.p==p)return &e;return nullptr;}
inline void paint(Entry& e){e.p->setMax(std::max(e.hi,display(e.hi)));e.p->setMin(display(e.lo));e.p->setMax(display(e.hi));e.p->setValue(display(e.value));e.p->setPrecision(inches()?2:0);e.p->setLabel(e.name+(inches()?" in":" mm"));}
inline ofxDatGuiSlider* track(ofxDatGuiSlider* p,std::string name,float lo,float hi,float mm){load();p->setName(name);entries().push_back({p,name,lo,hi,mm});paint(entries().back());return p;}
inline float read(ofxDatGuiSliderEvent e){auto p=find(e.target);float mm=p&&inches()?e.value*25.4f:e.value;if(p)p->value=mm;return mm;}
inline void value(ofxDatGuiSlider* p,float mm){auto e=find(p);if(e){e->value=mm;paint(*e);}else p->setValue(display(mm));}
inline void range(ofxDatGuiSlider* p,float lo,float hi,float mm){auto e=find(p);if(e){e->lo=lo;e->hi=hi;e->value=mm;paint(*e);}}
inline void minimum(ofxDatGuiSlider*p,float mm){auto e=find(p);if(e){e->lo=mm;paint(*e);}}
inline void maximum(ofxDatGuiSlider*p,float mm){auto e=find(p);if(e){e->hi=mm;paint(*e);}}
inline void setInches(bool yes){load();if(inches()==yes)return;inches()=yes;++revision();for(auto&e:entries())paint(e);try{ofJson j;j["inches"]=yes;ofSavePrettyJson(ofToDataPath("display-units.json",true),j);}catch(const std::exception&) {}}
}
