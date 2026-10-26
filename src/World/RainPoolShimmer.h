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
// SPDX-License-Identifier: GPL-2.0-or-later
/* PROJECT_ATTRIBUTION_V45
 * Pool-caustic artwork created with ChatGPT image generation for v28.
 * Subsequent distortion animates texture coordinates; it is not a separate asset.
 * See ASSETS.md and CREDITS.md. Existing notices remain in place.
 */
#pragma once
#include "ofMain.h"
#include <string>
#include <iostream>
#include <cmath>
// POOL_ANIMATION_V29: visible animated caustics, animated only inside the depth mask.
// No simulation changes, extra camera work, framebuffer copies or CPU particles.
// The animated caustics are a visual cue for deeper pooled water. They do not
// inject waves or move solver water; pool coverage still comes from the mesh.
// Keep the shader fallback for machines using the older OpenGL renderer.
class RainPoolShimmer {
public:
    void draw(ofMesh& mesh,float seconds){
        if(mesh.getNumVertices()==0)return;
        if(!tried)setup();
        if(ready){
            shader.begin();
            shader.setUniform1f("poolTime",seconds);
            shader.setUniformTexture("poolTexture",texture.getTexture(),0);
            shader.setUniform2f("textureSize",texture.getWidth(),texture.getHeight());
            mesh.draw();
            shader.end();
        }else if(loaded){
            // A visibly textured static fallback, including on shader failures.
            // UVs on the source mesh are grid coordinates, not image pixels.
            ofMesh fallback=mesh;fallback.clearTexCoords();
            for(auto& color:fallback.getColors())color.r=color.g=color.b=1.f;
            for(const auto& uv:mesh.getTexCoords()){
                const float u=mirror(uv.x*.04f),v=mirror(uv.y*.04f);
                fallback.addTexCoord(texture.getTexture().getCoordFromPercent(u,v));
            }
            texture.getTexture().bind();fallback.draw();texture.getTexture().unbind();
        }else mesh.draw();
    }
private:
    ofShader shader;ofImage texture;bool tried=false,loaded=false,ready=false;
    static float mirror(float x){return 1.f-std::abs(2.f*(x*.5f-std::floor(x*.5f))-1.f);}
    void setup(){
        tried=true;
        loaded=texture.load("images/rain-pool-caustics-v28.png");
        if(!loaded){
            std::cout<<"[RAIN POOL V29] texture=MISSING; expected bin/data/images/rain-pool-caustics-v28.png"<<std::endl;
            return;
        }
        const bool modern=ofIsGLProgrammableRenderer();
        const bool rectangle=texture.getTexture().getTextureData().textureTarget==GL_TEXTURE_RECTANGLE_ARB;
        const std::string vertex=modern?R"GLSL(#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
in vec4 color;
in vec2 texcoord;
out vec4 waterColor;
out vec2 waterUV;
void main(){waterColor=color;waterUV=texcoord;gl_Position=modelViewProjectionMatrix*position;}
)GLSL":R"GLSL(#version 120
varying vec4 waterColor;
varying vec2 waterUV;
void main(){waterColor=gl_Color;waterUV=gl_MultiTexCoord0.xy;gl_Position=ftransform();}
)GLSL";
        std::string fragment=modern?"#version 150\nin vec4 waterColor;\nin vec2 waterUV;\nout vec4 poolOutput;\n":
            "#version 120\nvarying vec4 waterColor;\nvarying vec2 waterUV;\n";
        fragment+=rectangle?"uniform sampler2DRect poolTexture;\n":"uniform sampler2D poolTexture;\n";
        fragment+="uniform vec2 textureSize;\nuniform float poolTime;\n";
        fragment+=R"GLSL(
vec2 mirrored(vec2 p){return 1.0-abs(2.0*fract(p*.5)-1.0);}
vec3 samplePool(vec2 p){
    vec2 uv=mirrored(p);
)GLSL";
        const std::string sample=modern?"texture":rectangle?"texture2DRect":"texture2D";
        fragment+="return "+sample+"(poolTexture,"+(rectangle?"uv*textureSize":"uv")+").rgb;\n}\n";
        fragment+=R"GLSL(
void main(){
    vec2 p=waterUV*.04;
    // Local distortion only: no texture rotation, orbit, or circular flow.
    vec2 bend=.035*vec2(sin(p.y*7.0+poolTime*.85),cos(p.x*8.0-poolTime*.73));
    bend+=.016*vec2(sin(p.x*13.0-p.y*9.0-poolTime*.61),cos(p.y*12.0+p.x*7.0+poolTime*.67));
    vec3 a=samplePool(p+bend);
    vec3 b=samplePool(p-bend*.55);
    vec3 lit=mix(a,b,.18)*1.08;
    // Light catches different reflection lines; shoreline/opacity stay fixed.
    float highlight=smoothstep(.55,.92,dot(lit,vec3(.2126,.7152,.0722)));
    lit*=1.0+highlight*.10*sin(p.x*10.0+p.y*8.0+poolTime*1.6);
    lit=clamp(lit,0.0,1.0);
)GLSL";
        fragment+=modern?"poolOutput=vec4(lit,waterColor.a);\n}\n":"gl_FragColor=vec4(lit,waterColor.a);\n}\n";
        const bool v=shader.setupShaderFromSource(GL_VERTEX_SHADER,vertex);
        const bool f=shader.setupShaderFromSource(GL_FRAGMENT_SHADER,fragment);
        if(modern)shader.bindDefaults();
        ready=v&&f&&shader.linkProgram();
        std::cout<<"[RAIN POOL V29] texture=loaded animation="<<(ready?"ready":"static_texture_fallback")
            <<" backend="<<(modern?"GL3":"GL2")<<" sampler="<<(rectangle?"rectangle":"normalized")
            <<"; depth controls coverage; physical rain unchanged"<<std::endl;
    }
};
