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
#pragma once
#include "ofMain.h"
#include <iostream>
#include <string>
// LAVA_DISTORTION_V30: alter texture sampling only. The original heat tint,
// coverage, mesh positions, physics, cooling, crust and smoke remain intact.
// Distortion changes sampled texture coordinates rather than rotating the
// entire image, avoiding a whirlpool appearance. Shader source strings below
// are executable assets; preserve their text when cleaning C++ comments.
class LavaSurfaceMotion {
public:
    void draw(ofVboMesh& mesh,ofImage& image,float seconds){
        if(mesh.getNumVertices()==0)return;
        if(!tried)setup(image);
        if(ready){
            shader.begin();
            shader.setUniform1f("lavaTime",seconds);
            shader.setUniformTexture("lavaTexture",image.getTexture(),0);
            shader.setUniform2f("coordinateSize",rectangle?image.getWidth():1.f,rectangle?image.getHeight():1.f);
            shader.setUniform2f("textureSize",image.getWidth(),image.getHeight());
            mesh.draw();
            shader.end();
        }else{
            image.getTexture().bind();mesh.draw();image.getTexture().unbind();
        }
    }
private:
    ofShader shader;bool tried=false,ready=false,rectangle=false;
    void setup(ofImage& image){
        tried=true;const bool modern=ofIsGLProgrammableRenderer();
        rectangle=image.getTexture().getTextureData().textureTarget==GL_TEXTURE_RECTANGLE_ARB;
        const std::string vertex=modern?R"GLSL(#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
in vec4 color;
in vec2 texcoord;
out vec4 lavaColor;
out vec2 lavaUV;
void main(){lavaColor=color;lavaUV=texcoord;gl_Position=modelViewProjectionMatrix*position;}
)GLSL":R"GLSL(#version 120
varying vec4 lavaColor;
varying vec2 lavaUV;
void main(){lavaColor=gl_Color;lavaUV=gl_MultiTexCoord0.xy;gl_Position=ftransform();}
)GLSL";
        std::string fragment=modern?"#version 150\nin vec4 lavaColor;\nin vec2 lavaUV;\nout vec4 lavaOutput;\n":
            "#version 120\nvarying vec4 lavaColor;\nvarying vec2 lavaUV;\n";
        fragment+=rectangle?"uniform sampler2DRect lavaTexture;\n":"uniform sampler2D lavaTexture;\n";
        fragment+="uniform vec2 coordinateSize;\nuniform vec2 textureSize;\nuniform float lavaTime;\n";
        fragment+=R"GLSL(
void main(){
    vec2 p=lavaUV/coordinateSize;
    // Local bending, never rotation or bulk movement. Fade the displacement
    // to zero at each original tile edge so adjacent UV tiles remain aligned.
    float edge=sin(clamp(p.x,0.0,1.0)*3.14159265)*sin(clamp(p.y,0.0,1.0)*3.14159265);
    vec2 bend=.027*vec2(sin(p.y*6.2831853+lavaTime*.55),cos(p.x*6.2831853-lavaTime*.47));
    bend+=.010*vec2(sin((p.x+p.y)*12.5663706-lavaTime*.37),cos((p.x-p.y)*12.5663706+lavaTime*.41));
    vec2 uv=clamp(p+edge*bend,vec2(0.0),vec2(1.0));
)GLSL";
        const std::string sample=modern?"texture":rectangle?"texture2DRect":"texture2D";
        fragment+="vec4 pixel="+sample+"(lavaTexture,"+(rectangle?"uv*textureSize":"uv")+")*lavaColor;\n";
        fragment+=modern?"lavaOutput=pixel;\n}\n":"gl_FragColor=pixel;\n}\n";
        const bool v=shader.setupShaderFromSource(GL_VERTEX_SHADER,vertex);
        const bool f=shader.setupShaderFromSource(GL_FRAGMENT_SHADER,fragment);
        if(modern)shader.bindDefaults();
        ready=v&&f&&shader.linkProgram();
        std::cout<<"[LAVA DISTORTION V30] animation="<<(ready?"ready":"original_texture_fallback")
            <<" backend="<<(modern?"GL3":"GL2")<<"; texture coordinates only"<<std::endl;
    }
};
