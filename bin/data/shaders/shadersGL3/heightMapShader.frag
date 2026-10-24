/* TopoPlay modification notice — 2026-10-23
 * Educational-fork shader changes; source-release notice added on this date.
 * Original notices are retained. See docs/MODIFICATIONS.md.
 */
/***********************************************************************
heightMapShader - Shader fragment to display color and contourlines.
Copyright (c) 2016 Thomas Wolf

-- adapted from SurfaceAddContourLines by Oliver Kreylos
Copyright (c) 2012 Oliver Kreylos

This file is part of the Magic Sand.

The Magic Sand is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Magic Sand is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Magic Sand; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#version 150

// Terrain blend update v1; based on Magic Sand GPL terrain shader.
out vec4 outputColor;
in float depthfrag;
in float elevationfrag;

uniform sampler2DRect heightColorMapSampler;
uniform sampler2DRect pixelCornerElevationSampler;
uniform float contourLineFactor;
uniform int drawContourLines;

// TEXTURE_EDITOR_V52: physical elevation maps into the original texture scale.
// Four binary-search steps locate one of twelve intervals. Default anchors are
// the identity map, preserving the existing rendering without extra texture reads.
uniform int useRealisticTextures;
uniform float materialHeights[13];
uniform float materialReference[13];
uniform float terrainBlendMm;
uniform vec2 heightColorMapTransformation;
// Integral of a smoothed hinge: averaging the original piecewise-linear
// material map over +/- r mm widens compressed transitions without moving
// saved anchors, the physical waterline, habitats, or gesture heights.
float softHinge(float x,float r){
    if(x<=-r)return 0.0;
    if(x>=r)return x;
    return (x+r)*(x+r)/(4.0*r);
}
float blendedMaterialHeight(float h){
    float r=terrainBlendMm;
    float result=materialReference[0]-softHinge(materialHeights[0]-h,r)
        +softHinge(h-materialHeights[12],r);
    for(int i=0;i<12;++i){
        float a=materialHeights[i],b=materialHeights[i+1];
        float t=0.0;
        if(h>=b+r)t=1.0;
        else if(h>a-r){
            if(b-a<0.001)t=clamp((h-a+r)/(2.0*r),0.0,1.0);
            else t=clamp((softHinge(h-a,r)-softHinge(h-b,r))/(b-a),0.0,1.0);
        }
        result+=(materialReference[i+1]-materialReference[i])*t;
    }
    return result;
}
float textureLayerHeight(float height){
    if(terrainBlendMm>0.01 && useRealisticTextures==1)return blendedMaterialHeight(height);
    if(height<=materialHeights[0])return height-materialHeights[0]+materialReference[0];
    if(height>=materialHeights[12])return height-materialHeights[12]+materialReference[12];
    int low=0,high=12;
    for(int step=0;step<4;++step){
        if(high-low>1){int middle=(low+high)/2;if(height<materialHeights[middle])high=middle;else low=middle;}
    }
    float t=(height-materialHeights[low])/max(0.001,materialHeights[high]-materialHeights[low]);
    return mix(materialReference[low],materialReference[high],clamp(t,0.0,1.0));
}

uniform float foothillRockPercent; // GUI appearance control, 0..100
uniform float snowCoveragePercent; // 0..100; 100 permits a solid upper snowcap
uniform float snowOpacityPercent; // 0..100; 100 permits fully opaque upper snow

uniform sampler2DRect sandyGrassTextureSampler;
uniform sampler2DRect grassTextureSampler;
uniform sampler2DRect spottyGrassTextureSampler;
uniform sampler2DRect rockTextureSampler;
uniform sampler2DRect mountainTextureSampler;
uniform sampler2DRect snowTextureSampler;

uniform vec2 sandyGrassTextureSize;
uniform vec2 grassTextureSize;
uniform vec2 spottyGrassTextureSize;
uniform vec2 rockTextureSize;
uniform vec2 mountainTextureSize;
uniform vec2 snowTextureSize;

// Existing material scales retained for this first test.
vec3 sandAt(vec2 p) {
    return texture(sandyGrassTextureSampler,
        mod(p * 4.2, sandyGrassTextureSize)).rgb;
}
vec3 grassAt(vec2 p) {
    return texture(grassTextureSampler,
        mod(p * 25.0 + vec2(71.0,39.0), grassTextureSize)).rgb;
}
vec3 upperGrassAt(vec2 p) {
    return texture(spottyGrassTextureSampler,
        mod(p * 25.0 + vec2(143.0,87.0), spottyGrassTextureSize)).rgb;
}
vec3 rockAt(vec2 p) {
    return texture(rockTextureSampler,
        mod(p * 17.5, rockTextureSize)).rgb;
}
vec3 mountainAt(vec2 p) {
    return texture(mountainTextureSampler,
        mod(p * 0.65, mountainTextureSize)).rgb;
}
vec3 snowAt(vec2 p) {
    return texture(snowTextureSampler,
        mod(p * 1.15, snowTextureSize)).rgb;
}
float hashCell(vec2 p) {
    return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453);
}
float patchNoise(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f*f*(3.0-2.0*f);
    return mix(mix(hashCell(cell),hashCell(cell+vec2(1,0)),f.x),
               mix(hashCell(cell+vec2(0,1)),hashCell(cell+vec2(1,1)),f.x),f.y);
}
// NATURAL_FOOTHILLS_V41: low foothills have only sparse small gravel.
// Rare larger outcrops appear with elevation. Rotated, warped coordinates and
// low occupancy keep the placement lattice from reading as rows of dots.
// The upper edge joins the mountain through rock patches, not a gray wash.
vec3 foothillAt(float h, vec2 p) {
    vec3 grass = upperGrassAt(p);
    // Lower values remove gravel and thin out boulders instead of tinting
    // the whole grass surface gray. Five is the previous v41 appearance.
    if (foothillRockPercent <= 0.0) return grass;
    float sparsity = clamp(foothillRockPercent/5.0,0.0,1.0);
    float gravel = smoothstep(mix(0.99,0.82,sparsity),
                             mix(1.0,0.92,sparsity),patchNoise(p / 3.2));
    gravel *= 0.45 + 0.55 * smoothstep(80.0,120.0,h);
    vec2 warped = vec2(0.8*p.x+0.6*p.y,-0.6*p.x+0.8*p.y);
    warped += 18.0 * vec2(patchNoise(p/113.0),patchNoise(p/97.0+vec2(31.0,7.0)));
    vec2 cell = floor(warped / 76.0);
    float seed = hashCell(cell);
    float boulder = 0.0;
    if (seed < 0.18*sparsity && h > 96.0) {
        vec2 center = vec2(0.25+0.50*hashCell(cell+vec2(17.0,3.0)),
                           0.25+0.50*hashCell(cell+vec2(5.0,29.0)));
        vec2 local = (fract(warped/76.0)-center) *
                     vec2(0.78+0.62*hashCell(cell+vec2(3.0,41.0)),1.0);
        float radius = (0.10+0.11*hashCell(cell+vec2(37.0,11.0))) *
                       smoothstep(96.0,124.0,h);
        radius *= 0.65+0.65*patchNoise(p/6.0);
        boulder = 1.0-smoothstep(radius-0.008,radius+0.008,length(local));
    }
    float mountain = 0.0;
    if (h > 122.0) {
        float spread = smoothstep(122.0,134.0,h)*sparsity;
        float edge = 1.15-1.30*spread;
        mountain = smoothstep(edge-0.10,edge+0.10,patchNoise(p/30.0));
    }
    float stone = max(gravel,max(boulder,mountain));
    // Higher slider values expand irregular rocky areas. At 100 the whole
    // foothill interior uses the mountain texture, with no new texture reads.
    float more = clamp((foothillRockPercent-5.0)/95.0,0.0,1.0);
    if (more > 0.0) {
        float edge = 1.15-1.30*more;
        float extra = smoothstep(edge-0.10,edge+0.10,patchNoise(p/8.0));
        stone = max(stone,extra);
    }
    if (stone <= 0.001) return grass;
    return mix(grass,mountainAt(p),stone);
}
// LAYERED_SNOW_V43: snow accumulates in irregular, stationary patches.
// At the lower edge it is sparse and translucent. Elevation increases coverage
// and opacity. A full coverage setting lets the upper patches merge completely.
// These are visual masks only: no terrain depth, mesh, simulation or snow asset
// is changed. Reuse the existing rock/snow materials and cached terrain draw.
vec3 snowyMountainAt(float h, vec2 p) {
    vec3 rock = mountainAt(p);
    if (snowCoveragePercent <= 0.0 || snowOpacityPercent <= 0.0) return rock;
    float rise = smoothstep(165.0,210.0,h);
    float amount = clamp(snowCoveragePercent/100.0,0.0,1.0);
    float field = 0.65*patchNoise(p/27.0)+
                  0.35*patchNoise(p/9.0+vec2(19.0,43.0));
    float edge = 1.0-amount*mix(0.10,1.0,rise);
    float cover = smoothstep(edge-0.10,edge+0.10,field);
    float exposed = smoothstep(0.56,0.72,patchNoise(p/49.0+vec2(67.0,13.0)));
    cover *= (1.0-exposed)*smoothstep(165.0,173.0,h);
    // FULL_SNOW_V44: retain thin lower snow, but let the last part of the
    // coverage control close summit gaps. At 100/100, the upper surface uses
    // snow alone instead of retaining mandatory bare rock or transparency.
    float capFill = smoothstep(90.0,100.0,snowCoveragePercent) *
                    smoothstep(175.0,190.0,h);
    cover = mix(cover,1.0,capFill);
    float strength = clamp(snowOpacityPercent/100.0,0.0,1.0);
    float opacity = mix(strength*mix(0.16,1.0,rise),strength,capFill);
    if (cover*opacity <= 0.001) return rock;
    return mix(rock,snowAt(p),cover*opacity);
}
// Existing lower material thresholds and the 165 mm snow onset are retained.
vec3 naturalTerrain(float h, vec2 p) {
    if (h < 5.0) return sandAt(p);
    if (h < 15.0) return mix(sandAt(p),grassAt(p),smoothstep(5.0,15.0,h));
    if (h < 51.0) return grassAt(p);
    if (h < 65.0) return mix(grassAt(p),upperGrassAt(p),smoothstep(51.0,65.0,h));
    if (h < 80.0) return upperGrassAt(p);
    if (h < 96.0) return mix(upperGrassAt(p),foothillAt(h,p),smoothstep(80.0,96.0,h));
    if (h < 116.0) return foothillAt(h,p);
    if (h < 134.0) return foothillAt(h,p);
    if (h < 165.0) return mountainAt(p);
    return snowyMountainAt(h,p);
}
void main()
{
    vec4 color = texture(heightColorMapSampler, vec2(depthfrag,0.5));
    if(useRealisticTextures==1 && terrainBlendMm>0.01){
        float offset=terrainBlendMm*heightColorMapTransformation.x;
        color=color*0.5+0.25*texture(heightColorMapSampler,vec2(depthfrag-offset,0.5))
            +0.25*texture(heightColorMapSampler,vec2(depthfrag+offset,0.5));
    }
    vec2 p = gl_FragCoord.xy;
    float h = -elevationfrag;
    // Calculate derivatives before divergent material branches.
    vec3 screenNormal = normalize(vec3(-dFdx(h)*0.65,-dFdy(h)*0.65,1.0));
    float relief = clamp(0.90 + 0.14 * dot(screenNormal,
                          normalize(vec3(-0.4,0.5,1.0))),0.90,1.04);
    float materialHeight=textureLayerHeight(h);
    if (materialHeight >= 0.0 && useRealisticTextures==1) {
        float patch = patchNoise(p / 95.0);
        // Material boundaries may cross zero; physical elevation remains unchanged.
        float landHeight = materialHeight + (patch-0.5)*6.0*smoothstep(0.0,8.0,materialHeight);
        vec3 natural = naturalTerrain(landHeight,p);
        float grassZone = smoothstep(5.0,18.0,landHeight) *
                          (1.0-smoothstep(74.0,96.0,landHeight));
        natural *= mix(vec3(1.0),vec3(0.94+0.12*patch,
                        0.96+0.08*patch,0.94+0.10*patch),grassZone);
        color.rgb = mix(color.rgb,natural,0.92*smoothstep(0.0,4.0,materialHeight));
        color.rgb *= relief;
    }
    if (drawContourLines == 1)
    {
        float c0 =
            floor(texture(
                pixelCornerElevationSampler,
                vec2(gl_FragCoord.x,
                     gl_FragCoord.y)).r
                  * contourLineFactor);

        float c1 =
            floor(texture(
                pixelCornerElevationSampler,
                vec2(gl_FragCoord.x + 1.0,
                     gl_FragCoord.y)).r
                  * contourLineFactor);

        float c2 =
            floor(texture(
                pixelCornerElevationSampler,
                vec2(gl_FragCoord.x,
                     gl_FragCoord.y + 1.0)).r
                  * contourLineFactor);

        float c3 =
            floor(texture(
                pixelCornerElevationSampler,
                vec2(gl_FragCoord.x + 1.0,
                     gl_FragCoord.y + 1.0)).r
                  * contourLineFactor);

        int edgeMask = 0;
        int numEdges = 0;

        if (c0 != c1)
        {
            edgeMask += 1;
            ++numEdges;
        }

        if (c2 != c3)
        {
            edgeMask += 2;
            ++numEdges;
        }

        if (c0 != c2)
        {
            edgeMask += 4;
            ++numEdges;
        }

        if (c1 != c3)
        {
            edgeMask += 8;
            ++numEdges;
        }

        if (
            numEdges > 2 ||
            edgeMask == 3 ||
            edgeMask == 12 ||
            (
                numEdges == 2 &&
                mod(
                    floor(gl_FragCoord.x) +
                    floor(gl_FragCoord.y),
                    2.0
                ) == 0.0
            )
        )
        {
            // Preserve contour positions, but let the terrain show through.
            color.rgb *= 0.48;
        }
    }

    outputColor = color;
}
