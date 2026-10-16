// SPDX-License-Identifier: Apache-2.0
#pragma once

// GroomLab's own hair shading in Viewport 2.0 (displayHairShading 1, the default): the lit strand
// lines and ribbons drawn with an effect of GroomLab's (HLSL for DirectX 11, OGSFX for the OpenGL
// Core Profile) instead of Maya's hair tube shading graph.
//
// The shading is the standalone app's lookdev hair, one source: engine/gpu/shaders/hair_bsdf.glsl,
// embedded at build time (Marschner's R, TT and TRT lobes in Karis 2016's closed forms, their widths
// from the roughness through d'Eon's fits, the fiber's absorption from the strands' hair color as
// aiStandardHair absorbs its base color, Zinke's dual scattering), calibrated against Arnold's aiStandardHair
// (tests/data/app/arnold_compare.py). Its lights are
// Viewport 2.0's own, read from the draw context before each draw (hairPreDraw: directional, point,
// spot and area lights, ambient lights; their color, intensity, decay and cone; light linking from
// the groom's ignored lights), so the hair lights as the scene does. The strand lines keep their
// width as coverage (the lines effect's hash, a coverage mask on multisampled targets) and the
// level of detail's uniforms (lineLod, lineBlend). Output is linear, scaled by pi as Viewport 2.0's
// own surfaces are (a Lambert there shows color x light), so the hair sits with them; Maya's color
// management turns it to the view.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MShaderManager.h>

namespace MHWRender {
class MDrawContext;
class MTexture;
}

namespace groomlab {
struct HairLook;
}

namespace groomlab::maya {

// The coverage's draw callbacks (groom_draw_override_shaders.cpp): the blend and depth states of the strand
// lines' draws in a color pass.
void coveragePreDraw(MHWRender::MDrawContext& context, const MHWRender::MRenderItemList& items, MHWRender::MShaderInstance*);
void coveragePostDraw(MHWRender::MDrawContext& context, const MHWRender::MRenderItemList& items, MHWRender::MShaderInstance*);
// Whether a draw is in a pass drawing the scene's colors (not shadows, depth, selection...).
bool colorPass(const MHWRender::MDrawContext& context);
// The strand lines' width as coverage (engine/gpu/shaders/line_coverage.glsl, embedded: the app's
// too) in an effect's words: HLSL's for DirectX 11, else GLSL's as they are.
const std::string& lineCoverageText(bool hlsl);

// What a groom's hair shaders share from draw to draw: the lights it ignores (light linking) and
// its density grid (self-shadowing).
struct HairGroomState {
    std::vector<MDagPath> ignoredLights;
    // The hair's density grid (line_frames.h runDensity, written by the GPU after each update that
    // moved the strands): its texture (rg: the optical density, the ambient occlusion), the first
    // cell's corner, the cell's size and the cells along each axis; `valid` once written.
    MHWRender::MTexture* volume = nullptr;
    float origin[3] = {0.0f, 0.0f, 0.0f};
    float cell = 1.0f;
    unsigned size[3] = {0, 0, 0};
    bool valid = false;
    double gpuMs = -1.0;     // the last grid's milliseconds on the GPU (-1: not known)
    std::uint64_t serial = 0;  // grids written so far
    std::string why;         // why there is none ("": there is, or none was asked for)
    HairGroomState() = default;
    HairGroomState(const HairGroomState&) = delete;
    HairGroomState& operator=(const HairGroomState&) = delete;
    ~HairGroomState();
    // The texture for a grid of `size` cells (made again for another size); null where none can be made.
    MHWRender::MTexture* volumeFor(const unsigned size[3]);
};

// The grid on a hair shader: its texture, where it is, and how opaque the hair is to its own light
// (displaySelfShadowing; 0, or no valid grid: no shadow, no occlusion).
void setHairGrid(MHWRender::MShaderInstance& shader, const HairGroomState* groom, float selfShadowing);

// The hair effect: technique Hair (the item's color, hairColor) or HairColored (the colors stream),
// or with `card` HairCard (game cards: the baked color map cut out by its coverage, parameters as
// the cards' own effect names them: map, cutoff, solid, solidColor, mapSize), with hairPreDraw and
// hairPostDraw (the lights; with `coverage` the strand lines' blending too). Null where it does not
// compile (said once) or the draw API takes none (legacy OpenGL).
MHWRender::MShaderInstance* acquireHairShader(const MHWRender::MShaderManager& shaders, bool colored, bool coverage,
                                              const std::shared_ptr<HairGroomState>& groom, bool card = false);
bool hairShaderAvailable(const MHWRender::MShaderManager& shaders);

// The hair's material on a hair shader: the color (its own, or the colors stream's: a hair color,
// hair_color.h, absorbed as aiStandardHair absorbs its base color) and HairColor's highlight
// (specular, roughness, tilt) as create_hair_shader gives it to aiStandardHair (core's hairLobes,
// hair_color.h). variation: the viewport's own per-strand variation (displayStrandVariation); debug
// (the optionVar groomLabHairDebug): 1 the tangents as colors, 2 each strand's random numbers, 3 the
// structural color's hue turned by each strand's random number.
void setHairMaterial(MHWRender::MShaderInstance& shader, const float color[4], const HairLook& look, float variation = 1.0f,
                     int debug = 0);

// The lights the groom at `path` ignores (light linking: MLightLinks), each light's path.
std::vector<MDagPath> ignoredLights(const MDagPath& path);

// Test hooks: the lights the last hair draw took (JSON: count, ambient, and each light's kind,
// direction or position, irradiance) and the hair draws so far.
std::string hairLightsJson();

}  // namespace groomlab::maya
