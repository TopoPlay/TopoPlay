#pragma once
// WATER_CREDITS_V69: this project renderer uses the MIT-attributed ripple
// algorithm in WaterRippleGrid.h plus project-specific hydraulic heights.
// Retain licenses/evan-water-MIT.txt when distributing the adapted component.
// This wrapper retains its own GPL notice; see docs/WATER-MOVEMENT.md.
// SPDX-License-Identifier: GPL-2.0-or-later
// WAVE_RENDER_V71: color-neutral ripple lighting, isolated GL state, stable hand reference.
// WHITE_REFLECTIONS_V73: additive light; parent color/alpha retained.
// V67_WATER_RETURN_V72: original small-ripple display strength and V67 solver.
// WAVE_CREST_V65: continuous sweep tracking, visible crest/trough and diagnostics.
// SHORE_WAVES_V64: depth-weighted waves, bank absorption and directional pushes.
// WATER_ECO_V63: cached geometry/material mapping; half-resolution lighting pass.
// CALM_WATER_V62: slower surface waves, sparse gentle rain impulses, subtle artwork.
// WATER_TILES_V61: softer shore fades and smaller blended caustic details.
// WATER_RIPPLES_V60: top-down water lighting over a small, interactive wave grid.
// Wave solver credit/license: WaterRippleGrid.h and licenses/evan-water-MIT.txt.
// SURGE_WATER_V66: hand momentum enters shared hydraulic storage; render its head.
// No extra depth capture or higher resolution lighting.
#include "ofMain.h"
#include "WaterRippleSettings.h"
#include "WaterRippleGrid.h"
#include "WaterSweep.h"
#include "AmbientWaterRipples.h"
#include "AnimalTerrain.h"
#include "RainWaterGrid.h"
#include "RainSurfaceLook.h"
#include "../KinectProjector/KinectProjector.h"
#include <memory>
#include <iostream>

class InteractiveWater {
public:
    void reset(){
        mesh.clear();waves=WaterRippleGrid();previousHand.clear();
        lastTime=-1;nextMesh=0;carry=0;lastSequence=0;hadFresh=false;
        shapeDirty=true;lastCorners.clear();sourceWidth=sourceHeight=0;
        handSampleAt=-1;handWeight=0;nextPush=0;terrainGuardUntil=-1;sweep.reset();ambient.reset();
    }
    // Suppress transient moving-hand terrain samples, not permanent mounds.
    // A stationary changed surface is eligible again after the short grace.
    bool protectsTerrainCell(int cell,float now)const{
        if(now>terrainGuardUntil||!waves.width||sourceWidth<1||sourceHeight<1||cell<0||cell>=sourceWidth*sourceHeight)return false;
        int x=std::min(waves.width-1,(cell%sourceWidth)*waves.width/sourceWidth);
        int y=std::min(waves.height-1,(cell/sourceWidth)*waves.height/sourceHeight);
        return previousHand[y*waves.width+x]>.03f;
    }
    void update(const std::shared_ptr<KinectProjector>& camera,const ofRectangle& roi,
                RainWaterGrid& water,const RainSurfaceLook& look,
                const std::vector<ofVec2f>& corners,const ofShortPixels& raw,
                uint64_t sequence,float rawAge,float now,bool raining,
                const std::vector<unsigned char>& rain,bool showSea){
        if(!MagicSandWaterRipples::enabled()){if(waves.width)reset();return;}
        if(!camera||water.width<2||water.height<2)return;
        if(!tried)setup();
        if(!ready)return;
        const int w=std::max(2,int(std::round(water.width*std::min(1.f,64.f/std::max(water.width,water.height)))));
        const int h=std::max(2,int(std::round(water.height*std::min(1.f,64.f/std::max(water.width,water.height)))));
        unsigned revision=MagicSandAnimalTerrain::profile().revision;
        if(waves.width!=w||waves.height!=h||profileRevision!=revision||sourceWidth!=water.width||sourceHeight!=water.height){
            reset();waves.setup(w,h);previousHand.assign(w*h,0);
            opacity.assign(w*h,0);depth.assign(w*h,0);source.assign(w*h,0);
            slopeX.assign(w*h,0);slopeY.assign(w*h,0);displayX.assign(w*h,0);displayY.assign(w*h,0);displayHeight.assign(w*h,0);handSurface.assign(w*h,0);handSurfaceValid.assign(w*h,0);
            cachedGround.assign(w*h,0);cachedPool.assign(w*h,0);cachedValid.assign(w*h,2);
            physicalCrest.assign(w*h,0);waveBed.assign(w*h,0);waveDepth.assign(w*h,0);waveValid.assign(w*h,0);
            sourceWidth=water.width;sourceHeight=water.height;cachedSea=showSea;
            for(int y=0;y<h;++y)for(int x=0;x<w;++x){
                int sx=std::min(water.width-1,int((x+.5f)*water.width/w));
                int sy=std::min(water.height-1,int((y+.5f)*water.height/h));
                source[y*w+x]=sy*water.width+sx;
            }
            pixels.allocate(w,h,OF_PIXELS_RGB);profileRevision=revision;
            std::cout<<"[WATER RIPPLES V60] grid="<<w<<"x"<<h
                <<" fixed=30Hz detail; V66 hydraulic crests and run-up"<<std::endl;
        }
        float dt=lastTime<0?0:ofClamp(now-lastTime,0.f,.1f);lastTime=now;
        // Reuse the terrain snapshot already acquired for runoff. Material-space
        // elevation follows edited water layers, including bands above physical 0.
        if(cachedSea!=showSea){cachedSea=showSea;std::fill(cachedValid.begin(),cachedValid.end(),2);}
        int wetCells=0;bool shoreDirty=false;
        const float seaLevel=MagicSandAnimalTerrain::profile().enabled?
            MagicSandAnimalTerrain::profile().levels[7]:0.f;
        for(int i=0;i<w*h;++i){
            const int j=source[i];
            float pool=j<int(look.shownDepth.size())?look.shownDepth[j]:0;
            // Sea baseline is already represented by the terrain water material.
            // Use live physical run-up on dry land, without a 0.65-second pool
            // appearance lag hiding a short crest. Thin-film saturation is kept.
            pool=water.baseline(j)>0?std::max(0.f,pool-water.baseline(j)):
                std::max(pool,water.water[j]);
            const float eta=water.ground[j]+water.water[j];
            float reference=seaLevel;
            if(water.baseline(j)<=0){
                reference=eta;float sum=eta;int count=1;
                for(int k:{j-1,j+1,j-water.width,j+water.width}){
                    if(k>=0&&k<int(water.water.size())&&water.valid[k]&&water.water[k]>.5f&&
                       (k/water.width==j/water.width||k%water.width==j%water.width)){
                        sum+=water.ground[k]+water.water[k];++count;
                    }
                }
                reference=sum/count;
            }
            physicalCrest[i]=water.water[j]>.3f?ofClamp((eta-reference)/12.f,-1.3f,1.3f):0.f;
            // Stable terrain usually occupies most of the box. Avoid repeated
            // material-space binary searches and smoothstep calculations there.
            if(cachedValid[i]!=water.valid[j]||cachedGround[i]!=water.ground[j]||cachedPool[i]!=pool){
                cachedValid[i]=water.valid[j];cachedGround[i]=water.ground[j];cachedPool[i]=pool;
                shoreDirty=true;waveBed[i]=water.ground[j];waveValid[i]=water.valid[j];
                waveDepth[i]=water.valid[j]?water.water[j]:0.f;
                float d=0,alpha=0;
                if(water.valid[j]){
                    float elevation=MagicSandAnimalTerrain::elevation(water.ground[j]);
                    float seaDepth=showSea?std::max(0.f,-elevation):0.f;
                    float seaAlpha=.65f*RainSurfaceLook::blend(.2f,5.f,seaDepth);
                    float poolAlpha=RainSurfaceLook::poolShade(pool).a;
                    d=std::max(seaDepth,pool);alpha=std::max(seaAlpha,poolAlpha);
                }
                if(d!=depth[i]||alpha!=opacity[i])shapeDirty=true;
                depth[i]=d;opacity[i]=alpha;waves.wet[i]=alpha>.008f;
            }
            if(waves.wet[i])++wetCells;
        }
        if(!wetCells){
            // No visible water: no shader, mesh rebuild, normal blur or upload.
            mesh.clear();shapeDirty=true;carry=0;lastSequence=sequence;hadFresh=false;
            std::fill(previousHand.begin(),previousHand.end(),0);
            handSampleAt=-1;handWeight=0;sweep.reset();
            std::fill(waves.displacement.begin(),waves.displacement.end(),0);
            std::fill(waves.velocity.begin(),waves.velocity.end(),0);
            return;
        }
        if(shoreDirty)waves.configureShore(waveBed,waveDepth,waveValid);
        // A swept hand leaves a signed depression/wake: remove the old footprint,
        // insert the new one. Holding still cannot pump energy continuously.
        // This is separate from the high-ceiling ring/rain gesture recognizer.
        const bool fresh=raw.isAllocated()&&rawAge<.45f;
        if(sequence!=lastSequence || (!fresh&&hadFresh)){
            lastSequence=sequence;
            float sum=0,sumX=0,sumY=0,overlap=0,maxClearance=0;int nearCells=0;
            const float surface=MagicSandAnimalTerrain::profile().enabled?
                MagicSandAnimalTerrain::profile().levels[7]:0.f;
            for(int y=0;y<h;++y)for(int x=0;x<w;++x){
                int i=y*w+x,j=source[i];float footprint=0;
                if(fresh&&water.valid[j]&&(water.baseline(j)>.2f||water.water[j]>.7f)){
                    int px=int(roi.x+(x+.5f)*roi.width/w),py=int(roi.y+(y+.5f)*roi.height/h);
                    if(px>=0&&py>=0&&px<int(raw.getWidth())&&py<int(raw.getHeight())){
                        float d=raw[py*raw.getWidth()+px];
                        if(d>=450&&d<=4500){
                            float e=camera->rainElevationFromDepth(px,py,d);
                            // Use the edited SEA waterline, not its passing crest. For a
                            // perched rain pond, follow its mean level slowly so a surge
                            // cannot continually move the gesture band under a hand.
                            float target=water.baseline(j)>0?surface:water.ground[j]+water.water[j];
                            if(!handSurfaceValid[i]){handSurface[i]=target;handSurfaceValid[i]=1;}
                            else handSurface[i]+=(target-handSurface[i])*(1-std::exp(-dt/.8f));
                            float base=water.baseline(j)>0?surface:handSurface[i];
                            float clearance=e-base;
                            if(std::isfinite(clearance)){
                                maxClearance=std::max(maxClearance,clearance);
                                if(clearance>8&&clearance<200){
                                    ++nearCells;
                                    footprint=RainSurfaceLook::blend(8,22,clearance)*(1-RainSurfaceLook::blend(160,200,clearance));
                                }
                            }
                        }
                    }
                }
                sum+=footprint;sumX+=(x+.5f)*footprint;sumY+=(y+.5f)*footprint;
                overlap+=std::min(previousHand[i],footprint);
                // Let the directional packet dominate instead of many sharp cell impacts.
                waves.disturb(i,(previousHand[i]-footprint)*.12f);
                previousHand[i]=footprint;
            }
            const float cx=sum>0?sumX/sum:0,cy=sum>0?sumY/sum:0;
            auto push=sweep.sample(now,cx,cy,fresh?sum:0,
                water.cellWidthMm()*water.width/w,water.cellHeightMm()*water.height/h);
            if(sum>=1.5f && (handWeight<1.5f || std::hypot(cx-handX,cy-handY)>.12f))terrainGuardUntil=now+.35f;
            handX=cx;handY=cy;handWeight=sum;
            if(push.fire){
                bool launched=water.push(push.x*water.width/w,push.y*water.height/h,
                    push.dx,push.dy,push.radius*std::sqrt(float(water.width*water.height)/(w*h)),push.strength);
                // Keep a legible surface ripple even when the hydraulic crest is
                // still travelling below it; these offsets add no actual water.
                waves.push(push.x,push.y,push.dx/(water.cellWidthMm()*water.width/w),
                    push.dy/(water.cellHeightMm()*water.height/h),push.radius,push.strength*.28f);
                ++pushCount;
                std::cout<<"[WAVE PUSH V73] speedMmSec="<<push.speed<<" strength="<<push.strength
                    <<" hydraulic="<<launched<<std::endl;
            }
            if(now>=nextHandReport&&nearCells){
                nextHandReport=now+3;
                std::cout<<"[WAVE HAND V73] fresh="<<fresh<<" areaCells="<<sum
                    <<" maxClearanceMm="<<maxClearance<<" pushes="<<pushCount<<std::endl;
            }
        }
        hadFresh=fresh;
        carry=std::min(.1f,carry+dt);
        // Keep 30 small visual updates per second for smoothness; integrate each
        // at 40% of the old speed instead of lowering the display/update FPS.
        while(carry>=1.f/30.f){waves.step(.4f);carry-=1.f/30.f;}
        // Sparse, small impulses add life to undisturbed water; rain only changes
        // the visual ripple frequency. It never changes actual rainfall volume.
        ambient.update(now,waves,depth);
        if(raining && now>=nextDrop){
            nextDrop=now+(raining?.65f:2.5f);
            for(int tries=0;tries<8;++tries){
                unsigned j=randomCell()%unsigned(w*h);
                if(waves.wet[j]&&(!raining||rain[source[j]])){
                    // Thin runoff should look saturated, not like a vibrating speaker.
                    // Depth only scales this visual impulse, never rain volume.
                    float depthGain=.2f+.8f*RainSurfaceLook::blend(.7f,8.f,depth[j]);
                    waves.disturb(int(j),(raining?.028f:.018f)*depthGain);break;
                }
            }
        }
        // WATER_TILES_V61: smooth only the display normals. The wave heights,
        // hand impulses, rain volume and drainage keep their existing values.
        // One small CPU blur avoids four extra shader fetches per projector pixel.
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){
            int i=y*w+x;
            float shoal=1.f+.4f*(1.f-waves.propagation[i]);
            auto physicalSlope=[&](int dx,int dy){
                int xx=std::max(0,std::min(w-1,x+dx)),yy=std::max(0,std::min(h-1,y+dy));
                int bx=std::max(0,std::min(w-1,x-dx)),by=std::max(0,std::min(h-1,y-dy));
                return .5f*(physicalCrest[yy*w+xx]-physicalCrest[by*w+bx]);
            };
            slopeX[i]=waves.slope(x,y,1,0)*2.5f*shoal+physicalSlope(1,0)*1.4f;
            slopeY[i]=waves.slope(x,y,0,1)*2.5f*shoal+physicalSlope(0,1)*1.4f;
        }
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){
            int i=y*w+x;
            auto soft=[&](const std::vector<float>& slopes){
                float sum=slopes[i]*.5f;
                const int dx[]={-1,1,0,0},dy[]={0,0,-1,1};
                for(int n=0;n<4;++n){
                    int xx=x+dx[n],yy=y+dy[n];
                    int j=(xx>=0&&yy>=0&&xx<w&&yy<h)?yy*w+xx:i;
                    sum+=(waves.wet[j]?slopes[j]:slopes[i])*.125f;
                }
                return sum;
            };
            // A short display-only low-pass prevents depth/8-bit normal jitter
            // becoming flashes of specular light. Physics/gestures are unfiltered.
            float follow=1-std::exp(-dt/.12f);
            displayX[i]+=(soft(slopeX)-displayX[i])*follow;
            displayY[i]+=(soft(slopeY)-displayY[i])*follow;
            displayHeight[i]+=(waves.displacement[i]*.75f+physicalCrest[i]*.75f-displayHeight[i])*follow;
            pixels[i*3]=encode(displayX[i]);
            pixels[i*3+1]=encode(displayY[i]);
            // More height precision makes a single crest/trough legible after projection.
            pixels[i*3+2]=encode(displayHeight[i]);
        }
        normalTexture.loadData(pixels);
        if(now>=nextMesh){
            nextMesh=now+.2f;
            if(shapeDirty||cornersChanged(corners)){
                buildMesh(water,corners);lastCorners=corners;shapeDirty=false;
            }
        }
    }
    // Add neutral reflections above the existing water colors. The boolean
    // reports availability; base pool/terrain rendering stays in the caller.
    bool draw(float seconds){
        if(!MagicSandWaterRipples::enabled()||!ready||!waves.width)return false;
        if(!mesh.getNumVertices())return true;
        DrawState restoreState;
        // A few isolated water cells are cheaper to shade directly than to
        // clear and composite a buffer covering the whole projector.
        if(mesh.getNumVertices()<384){drawLit(seconds);return true;}
        const auto viewport=ofGetCurrentViewport();
        const float width=viewport.width,height=viewport.height;
        if(!std::isfinite(width)||!std::isfinite(height)||width<2||height<2){drawLit(seconds);return true;}
        const float scale=std::min(.5f,std::min(640.f/width,360.f/height));
        const int w=std::max(2,int(std::round(width*scale)));
        const int h=std::max(2,int(std::round(height*scale)));
        if(w!=surfaceWidth||h!=surfaceHeight){
            surfaceWidth=w;surfaceHeight=h;
            ofFbo::Settings settings;
            settings.width=w;settings.height=h;settings.internalformat=GL_RGBA;
            settings.useDepth=false;settings.useStencil=false;settings.numSamples=0;
            settings.minFilter=GL_LINEAR;settings.maxFilter=GL_LINEAR;
            surface.allocate(settings);
            std::cout<<"[WATER ECO V63] lighting="<<w<<"x"<<h
                <<" output="<<width<<"x"<<height
                <<(surface.isAllocated()?" smoothed; same wave timing":" allocation failed; direct fallback")<<std::endl;
        }
        if(!surface.isAllocated()){drawLit(seconds);return true;}
        // Shade only a quarter as many pixels at 1280x720. A cheap bilinear
        // composite restores projector size; geometry remains in calibrated
        // projector coordinates, so neither ROI nor calibration changes.
        const GLboolean hadDepth=glIsEnabled(GL_DEPTH_TEST),hadCull=glIsEnabled(GL_CULL_FACE),hadStencil=glIsEnabled(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);
        ofPushStyle();
        surface.begin();ofClear(0,0,0,0);ofSetupScreenOrtho(width,height);
        ofSetColor(255);ofEnableAlphaBlending();
        // Store reflected light in RGB with zero alpha. Add it once at the
        // original size; never replace the parent image's color or alpha.
        glBlendEquation(GL_FUNC_ADD);glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
        drawLit(seconds);surface.end();
        ofSetColor(255);ofEnableAlphaBlending();
        glBlendEquation(GL_FUNC_ADD);glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
        surface.draw(0,0,width,height);ofPopStyle();
        if(hadDepth)glEnable(GL_DEPTH_TEST);if(hadCull)glEnable(GL_CULL_FACE);if(hadStencil)glEnable(GL_STENCIL_TEST);
        return true;
    }
private:
    struct DrawState {
        bool legacy;GLint program=0,active=0,tex2D=0,texRect=0;
        GLboolean depth=0,cull=0,stencil=0,scissor=0,blend=0,depthWrite=0,colorWrite[4]{};
        GLint srcRGB=0,dstRGB=0,srcAlpha=0,dstAlpha=0,eqRGB=0,eqAlpha=0;
        DrawState():legacy(!ofIsGLProgrammableRenderer()){
            ofPushStyle();ofPushView();glGetIntegerv(GL_CURRENT_PROGRAM,&program);
            if(legacy){glPushAttrib(GL_ALL_ATTRIB_BITS);glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);}
            else{
                depth=glIsEnabled(GL_DEPTH_TEST);cull=glIsEnabled(GL_CULL_FACE);
                stencil=glIsEnabled(GL_STENCIL_TEST);scissor=glIsEnabled(GL_SCISSOR_TEST);blend=glIsEnabled(GL_BLEND);
                glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);glGetBooleanv(GL_COLOR_WRITEMASK,colorWrite);
                glGetIntegerv(GL_BLEND_SRC_RGB,&srcRGB);glGetIntegerv(GL_BLEND_DST_RGB,&dstRGB);
                glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
                glGetIntegerv(GL_BLEND_EQUATION_RGB,&eqRGB);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqAlpha);
                glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);
                glGetIntegerv(GL_TEXTURE_BINDING_2D,&tex2D);glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB,&texRect);
            }
            glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);glDisable(GL_SCISSOR_TEST);
            glDepthMask(GL_FALSE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            if(legacy){glDisable(GL_ALPHA_TEST);glDisable(GL_COLOR_LOGIC_OP);}
            glBlendEquation(GL_FUNC_ADD);glEnable(GL_BLEND);
        }
        static void enable(GLenum flag,GLboolean value){if(value)glEnable(flag);else glDisable(flag);}
        ~DrawState(){
            ofPopView();ofPopStyle();
            if(legacy){glPopClientAttrib();glPopAttrib();}
            else{
                glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,tex2D);glBindTexture(GL_TEXTURE_RECTANGLE_ARB,texRect);glActiveTexture(active);
                glBlendEquationSeparate(eqRGB,eqAlpha);glBlendFuncSeparate(srcRGB,dstRGB,srcAlpha,dstAlpha);
                glDepthMask(depthWrite);glColorMask(colorWrite[0],colorWrite[1],colorWrite[2],colorWrite[3]);
                enable(GL_DEPTH_TEST,depth);enable(GL_CULL_FACE,cull);enable(GL_STENCIL_TEST,stencil);
                enable(GL_SCISSOR_TEST,scissor);enable(GL_BLEND,blend);
            }
            glUseProgram(program);
        }
    };
    void drawLit(float seconds){
        // Add only reflected light. Parent alpha is preserved, so ripple
        // opacity cannot alter subsequent clouds/trees or the window composite.
        glBlendEquation(GL_FUNC_ADD);glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ZERO,GL_ONE);
        shader.begin();
        // A single texture unit carries normals, not colored water artwork.
        shader.setUniformTexture("normalTexture",normalTexture,0);
        shader.setUniform2f("normalSize",waves.width,waves.height);
        shader.setUniform1f("waterTime",seconds);
        mesh.draw();shader.end();
    }
    bool cornersChanged(const std::vector<ofVec2f>& corners)const{
        if(corners.size()!=lastCorners.size())return true;
        for(size_t i=0;i<corners.size();++i)
            if(corners[i].x!=lastCorners[i].x||corners[i].y!=lastCorners[i].y)return true;
        return false;
    }
    WaterRippleGrid waves;WaterSweep sweep;AmbientWaterRipples ambient;
    std::vector<float> previousHand,opacity,depth,slopeX,slopeY,displayX,displayY,displayHeight;
    std::vector<float> cachedGround,cachedPool,cornerAlpha,cornerDepth,waveBed,waveDepth,physicalCrest;
    std::vector<unsigned char> waveValid;
    float handX=0,handY=0,handWeight=0,handSampleAt=-1,nextPush=0,terrainGuardUntil=-1;
    std::vector<float> handSurface;std::vector<unsigned char> handSurfaceValid;
    unsigned pushCount=0;float nextHandReport=0;
    std::vector<unsigned char> cachedValid;
    std::vector<ofVec2f> lastCorners;
    bool shapeDirty=true,cachedSea=true;
    int sourceWidth=0,sourceHeight=0,surfaceWidth=0,surfaceHeight=0;
    ofFbo surface;
    std::vector<int> source;
    ofVboMesh mesh;ofPixels pixels;ofTexture normalTexture;
    ofShader shader;
    bool tried=false,ready=false,hadFresh=false;
    float lastTime=-1,nextMesh=0,carry=0,nextDrop=0;
    uint64_t lastSequence=0;unsigned profileRevision=~0u,random=74119;
    unsigned randomCell(){random=random*1664525u+1013904223u;return random;}
    static unsigned char encode(float value){return static_cast<unsigned char>(ofClamp(127.5f+127.5f*value,0.f,255.f));}
    void buildMesh(const RainWaterGrid& water,const std::vector<ofVec2f>& corners){
        mesh.clear();mesh.setMode(OF_PRIMITIVE_TRIANGLES);
        if(corners.size()!=size_t((water.width+1)*(water.height+1)))return;
        // Compute one continuous corner mask, including dry neighbours. Skipping
        // a tile solely because its centre is dry cuts a half-opaque shared edge
        // off as a visible square. Drawing through zero-alpha corners fixes that
        // without increasing the sensor/physics/wave resolution.
        const int stride=waves.width+1;
        cornerAlpha.assign(stride*(waves.height+1),0);
        cornerDepth.assign(cornerAlpha.size(),0);
        for(int y=0;y<=waves.height;++y)for(int x=0;x<=waves.width;++x){
            float alpha=0,d=0;int count=0;
            for(int yy=y-1;yy<=y;++yy)for(int xx=x-1;xx<=x;++xx){
                if(xx<0||yy<0||xx>=waves.width||yy>=waves.height)continue;
                int i=yy*waves.width+xx;
                alpha+=opacity[i];d+=depth[i];++count;
            }
            if(count){cornerAlpha[y*stride+x]=alpha/count;cornerDepth[y*stride+x]=d/count;}
        }
        auto vertex=[&](int x,int y){
            float sx=float(x)*water.width/waves.width,sy=float(y)*water.height/waves.height;
            int ix=std::min(water.width-1,int(sx)),iy=std::min(water.height-1,int(sy));
            float tx=sx-ix,ty=sy-iy;int c=iy*(water.width+1)+ix;
            ofVec2f a=corners[c]*(1-tx)+corners[c+1]*tx;
            ofVec2f b=corners[c+water.width+1]*(1-tx)+corners[c+water.width+2]*tx;
            ofVec2f p=a*(1-ty)+b*ty;
            mesh.addVertex(ofVec3f(p.x,p.y,0));
            mesh.addTexCoord(ofVec2f(float(x)/waves.width,float(y)/waves.height));
            int corner=y*stride+x;
            // Terrain already supplies regular-water color; the rain-pool base
            // mesh supplies its own tint. Add only transparent light/shade.
            mesh.addColor(ofFloatColor(1,1,1,cornerAlpha[corner]));
        };
        for(int y=0;y<waves.height;++y)for(int x=0;x<waves.width;++x){
            int j=source[y*waves.width+x];if(!water.valid[j])continue;
            int c=y*stride+x;
            float alpha=std::max(std::max(cornerAlpha[c],cornerAlpha[c+1]),
                                 std::max(cornerAlpha[c+stride],cornerAlpha[c+stride+1]));
            if(alpha<=.002f)continue;
            vertex(x,y);vertex(x+1,y);vertex(x,y+1);
            vertex(x+1,y);vertex(x+1,y+1);vertex(x,y+1);
        }
    }
    void setup(){
        tried=true;
        // No image is required: normals light whatever water color is below.
        ofPixels initial;initial.allocate(2,2,OF_PIXELS_RGB);initial.set(128);
        normalTexture.loadData(initial);normalTexture.setTextureMinMagFilter(GL_LINEAR,GL_LINEAR);
        const bool modern=ofIsGLProgrammableRenderer();
        const bool rectangle=normalTexture.getTextureData().textureTarget==GL_TEXTURE_RECTANGLE_ARB;
        const std::string vertex=modern?R"GLSL(#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;in vec4 color;in vec2 texcoord;
out vec4 waterColor;out vec2 waterUV;
void main(){waterColor=color;waterUV=texcoord;gl_Position=modelViewProjectionMatrix*position;}
)GLSL":R"GLSL(#version 120
varying vec4 waterColor;varying vec2 waterUV;
void main(){waterColor=gl_Color;waterUV=gl_MultiTexCoord0.xy;gl_Position=ftransform();}
)GLSL";
        std::string fragment=modern?"#version 150\nin vec4 waterColor;in vec2 waterUV;out vec4 waterOutput;\n":
            "#version 120\nvarying vec4 waterColor;varying vec2 waterUV;\n";
        fragment+=rectangle?"uniform sampler2DRect normalTexture;\n":
            "uniform sampler2D normalTexture;\n";
        fragment+="uniform vec2 normalSize;uniform float waterTime;\n";
        std::string sample=modern?"texture":rectangle?"texture2DRect":"texture2D";
        fragment+="vec3 normalAt(vec2 uv){return "+sample+"(normalTexture,"+
            (rectangle?std::string("clamp(uv*normalSize,vec2(.5),normalSize-vec2(.5))"):std::string("clamp(uv,.5/normalSize,1.0-.5/normalSize)"))+").rgb*2.0-1.0;}\n";
        fragment+=R"GLSL(
void main(){
    vec3 wave=normalAt(waterUV);
    vec2 p=waterUV*vec2(7.5,5.4);
    vec2 breeze=.003*vec2(sin(p.y*9.0+waterTime*.24),cos(p.x*8.0-waterTime*.21));
    vec3 normal=normalize(vec3(-(wave.xy+breeze)*3.8,1.0));
    vec3 halfway=normalize(vec3(.38,-.24,1.0));
    float facing=max(0.0,dot(normal,halfway));
    // Subtract flat-water lighting: zero disturbance adds no replacement color.
    // Broader highlights are smoother at the existing 15 FPS display rate.
    float glint=pow(facing,24.0)-pow(halfway.z,24.0);
    float crest=smoothstep(.045,.18,wave.z);
    float trough=smoothstep(.035,.20,-wave.z);
    float light=max(glint,0.0)*.48+crest*.22;
    // White reflection only: no blue artwork, black shading, or replacement
    // water alpha. Broad highlights remain legible at the saved 15 FPS.
    float reflection=clamp(light,0.0,.48)*waterColor.a;
    vec3 lit=vec3(reflection);
    float alpha=0.0;
)GLSL";
        fragment+=modern?"waterOutput=vec4(lit,alpha);\n}\n":
            "gl_FragColor=vec4(lit,alpha);\n}\n";
        bool v=shader.setupShaderFromSource(GL_VERTEX_SHADER,vertex);
        bool f=shader.setupShaderFromSource(GL_FRAGMENT_SHADER,fragment);
        if(modern)shader.bindDefaults();ready=v&&f&&shader.linkProgram();
        std::cout<<"[WAVE RENDER V76] "<<(ready?"ready":"shader failed; previous water retained")
            <<" backend="<<(modern?"GL3":"GL2")<<std::endl;
    }
};
