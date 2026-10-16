// SPDX-License-Identifier: Apache-2.0
// The draw override's GPU work: the lit lines' frames, the ribbons, following the scalp, the look
// preview, the hair's density grid, and their test hooks.

#include "groom_draw_override_internal.h"
#include "gpu_lines.h"
#include "gpu_preview.h"
#include "hair_shading.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include <maya/MBoundingBox.h>
#include <maya/MFrameContext.h>
#include <maya/MGlobal.h>
#include <maya/MHWGeometry.h>
#include <maya/MShaderManager.h>
#include <maya/MTextureManager.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <GL/gl.h>

#include "display_theme.h"
#include "groom_shape.h"
#include "maya_inputs.h"
#include "groomlab/cards.h"
#include "groomlab/color_patterns.h"
#include "groomlab/display.h"
#include "groomlab/hair_color.h"
#include "groomlab/instances.h"
#include "groomlab/parallel.h"
#include "groomlab/random.h"
#include "groomlab/scalp_paint.h"
#include "groomlab/texture.h"

namespace groomlab::maya {

namespace {

// An OpenGL vertex buffer's name: Viewport 2.0's resource handle there points at it.
void* glBufferName(MHWRender::MVertexBuffer* buffer) {
    const auto* name = buffer ? static_cast<const GLuint*>(buffer->resourceHandle()) : nullptr;
    return name ? reinterpret_cast<void*>(static_cast<std::uintptr_t>(*name)) : nullptr;
}

}  // namespace

bool GroomSubSceneOverride::writeGpuLines(ItemBuffers& b, const StrandLines& lines,
                                          const std::vector<std::pair<std::size_t, std::size_t>>* runs, bool withUvs,
                                          double* megabytes, double* ms) {
    gpu::LineFrames* frames = gpuLines_ ? gpuLines_->engine() : nullptr;
    if (!frames || !lines.strands || !lines.lit || lines.strands->pointCount() == 0) return false;
    try {
        return writeGpuLines(*frames, b, lines, runs, withUvs, megabytes, ms);
    } catch (const std::exception& e) {
        // The device would not (a kernel, a buffer): the CPU writes the frames from now on (the
        // item written whole by the caller, its streams dropped).
        gpuLines_->fail(e.what());
        MGlobal::displayWarning(MString("GroomLab: the lit strands' frames are written on the CPU (") + e.what() + ")");
        return false;
    }
}

bool GroomSubSceneOverride::writeGpuLines(gpu::LineFrames& engine, ItemBuffers& b, const StrandLines& lines,
                                          const std::vector<std::pair<std::size_t, std::size_t>>* runs, bool withUvs,
                                          double* megabytes, double* ms) {
    gpu::LineFrames* frames = &engine;
    const StrandSet& s = *lines.strands;
    const auto count = static_cast<unsigned>(s.pointCount());
    // Following the scalp on the GPU: the lines are the strands at rest, carried into the points below.
    const bool follow = follow_ && !runs;
    void* targets[4] = {};
    const bool gl = frames->api() == gpu::GpuApi::OpenGL;
    if (!gl) {
        if (!b.gpuStreams || b.gpuStreams->device != frames->device() || b.gpuStreams->count != count) {
            if (runs) return false;  // nothing held to write part of
            std::unique_ptr<MHWRender::MVertexBuffer>* vertices[4] = {&b.positions, &b.normals, &b.tangents, &b.uvs};
            b.gpuStreams = gpuLines_->makeStreams(count, b.gpuStreams, vertices);  // (the old MVertexBuffers went first)
            b.uvsKey = b.uvsIdsKey = 0;
        }
        for (int i = 0; i < 4; ++i) targets[i] = b.gpuStreams->buffers[i];
    } else {
        // OpenGL: Maya's own buffers (GPU-only there). Written in part: they hold these points.
        // Whole: made for them here where they are not, the frames' with no data (the kernel
        // writes them), the points' and uvs' below.
        std::unique_ptr<MHWRender::MVertexBuffer>* own[4] = {&b.positions, &b.normals, &b.tangents, &b.uvs};
        static const MHWRender::MGeometry::Semantic kSemantics[4] = {MHWRender::MGeometry::kPosition, MHWRender::MGeometry::kNormal,
                                                                      MHWRender::MGeometry::kTangent, MHWRender::MGeometry::kTexture};
        for (int i = 0; i < 4; ++i) {
            std::unique_ptr<MHWRender::MVertexBuffer>& v = *own[i];
            if (v && v->hasCustomResourceHandle()) return false;
            const bool fits = v && v->vertexCount() == count;
            if (runs && !fits) return false;
            if (fits) continue;
            if (!v) {
                v = std::make_unique<MHWRender::MVertexBuffer>(
                    MHWRender::MVertexBufferDescriptor("", kSemantics[i], MHWRender::MGeometry::kFloat, i < 3 ? 3 : 2), false);
            }
            if (i == 1 || i == 2 || (i == 0 && follow)) {  // (the kernels write them)
                void* memory = v->acquire(count, true);
                if (!memory) throw std::runtime_error("out of video memory for the frames");
                v->commit(memory);
            }
            if (i == 3) b.uvsKey = b.uvsIdsKey = 0;  // written whole below
        }
    }
    double bytes = 0;
    Lap lap;
    const Vec3f* points = s.positions().data();
    // The points: whole, or the changed strands' (each run one write, from the strands' own
    // array). DirectX 11: into the streams (UpdateSubresource); OpenGL: through Maya's buffer as
    // the CPU's writes go (a glBufferSubData of its own into a buffer the last frame drew stalls).
    if (follow) {
        // (carried below, carryOnGpu)
    } else if (runs) {
        for (const auto& [first, end] : *runs) {
            const std::size_t begin = s.strandBegin(first), n = s.offsets()[end] - begin;
            if (gl) {
                b.positions->update(points + begin, static_cast<unsigned>(begin), static_cast<unsigned>(n), false);
            } else {
                frames->write(targets[0], points + begin, begin * 12, n * 12);
            }
            bytes += static_cast<double>(n) * 12;
        }
    } else if (gl) {
        auto* out = static_cast<float*>(b.positions->acquire(count, true));
        if (!out) throw std::runtime_error("out of video memory for the positions");
        lines.writePositions(out);
        b.positions->commit(out);
        bytes += static_cast<double>(count) * 12;
    } else {
        frames->write(targets[0], points, 0, std::size_t{count} * 12);
        bytes += static_cast<double>(count) * 12;
    }
    if (gl) {
        MHWRender::MVertexBuffer* own[4] = {b.positions.get(), b.normals.get(), b.tangents.get(), b.uvs.get()};
        for (int i = 0; i < 3; ++i) {
            targets[i] = glBufferName(own[i]);
            if (!targets[i]) throw std::runtime_error("the viewport's buffers have no OpenGL names");
        }
    }
    ms[0] = lap();
    // The layout and each strand's root normal, when they changed; then the frames, all of them.
    bytes += static_cast<double>(frames->setLayout(s.offsets().data(), s.strandCount(), lines.layoutKey()));
    ms[1] = lap();
    if (follow) {
        // The points carried from the strands at rest, with their root normals on the scalp now.
        double carried = 0;
        carryOnGpu(*frames, b, targets[0], lines.layoutKey(), &carried);
        bytes += carried * 1e6;
        ms[2] = lap();
    } else {
        thread_local std::vector<float> up;
        up.resize(s.strandCount() * 4);
        const std::uint64_t upKey = lines.writeRootNormals(up.data());
        ms[2] = lap();
        bytes += static_cast<double>(frames->setRootNormals(up.data(), upKey));
    }
    ms[3] = lap();
    frames->run(targets[0], targets[1], targets[2]);
    ms[4] = lap();
    // The uvs (u with the strand's number, width): kept while their counts, IDs and widths are;
    // the widths alone where only they changed; else written (whole, or the changed strands').
    const std::uint64_t uvsKey = hashCombine(lines.layoutKey(), lines.widthsKey()) | 1u;
    const bool idsHeld = b.uvsIdsKey == lines.idsKey();
    const float* widths = s.widths().size() == s.pointCount() ? s.widths().data() : nullptr;
    if (b.uvsKey == uvsKey && idsHeld) {
        // as they are
    } else if (runs) {
        // (The other strands' uvs are as they were: only the changed strands differ.)
        if (withUvs) {
            thread_local std::vector<float> scratch;
            for (const auto& [first, end] : *runs) {
                const std::size_t begin = s.strandBegin(first), n = s.offsets()[end] - begin;
                scratch.resize(n * 2);
                lines.writeUvs(scratch.data(), 2, first, end);
                if (gl) {
                    b.uvs->update(scratch.data(), static_cast<unsigned>(begin), static_cast<unsigned>(n), false);
                } else {
                    frames->write(targets[3], scratch.data(), begin * 8, n * 8);
                }
                bytes += static_cast<double>(n) * 8;
            }
            b.uvsKey = uvsKey;
            b.uvsIdsKey = lines.idsKey();
        }
        // (else: widths the shader does not read, left as they were: written whole when it does)
    } else if (idsHeld && widths && lines.widthsKey() != 0 && (!gl || glBufferName(b.uvs.get()))) {
        bytes += static_cast<double>(frames->writeWidths(gl ? glBufferName(b.uvs.get()) : targets[3], widths));
        b.uvsKey = uvsKey;
    } else if (gl) {
        auto* out = static_cast<float*>(b.uvs->acquire(count, true));
        if (!out) throw std::runtime_error("out of video memory for the uvs");
        lines.writeUvs(out, 2);
        b.uvs->commit(out);
        bytes += static_cast<double>(count) * 8;
        b.uvsKey = uvsKey;
        b.uvsIdsKey = lines.idsKey();
    } else {
        // (Written straight into the memory that goes up, where the engine gives it.)
        if (auto* out = static_cast<float*>(frames->beginWrite(targets[3], 0, std::size_t{count} * 8))) {
            lines.writeUvs(out, 2);
            frames->endWrite();
        } else {
            thread_local std::vector<float> scratch;
            scratch.resize(std::size_t{count} * 2);
            lines.writeUvs(scratch.data(), 2);
            frames->write(targets[3], scratch.data(), 0, std::size_t{count} * 8);
        }
        bytes += static_cast<double>(count) * 8;
        b.uvsKey = uvsKey;
        b.uvsIdsKey = lines.idsKey();
    }
    *megabytes = bytes / 1e6;
    ms[5] = lap();
    return true;
}

void GroomSubSceneOverride::dropGpuRibbons(ItemBuffers& buffers) {
    buffers.ribbonPoints.reset();
    buffers.ribbonUvs.reset();
    if (buffers.ribbonStreams) {  // (everything the item draws names them)
        buffers.positions.reset();
        buffers.normals.reset();
        buffers.tangents.reset();
        buffers.uvs.reset();
        buffers.colors.reset();
        buffers.indices.reset();
        buffers.ribbonStreams.reset();  // after the buffers naming them
    }
    buffers.ribbonLayout = buffers.ribbonUvsKey = buffers.ribbonColorsKey = buffers.ribbonIdsKey = 0;
    buffers.followRest = buffers.followTo = 0;
}

void GroomSubSceneOverride::gpuRibbonTargets(gpu::LineFrames& engine, const ItemBuffers& b, void* t[8]) {
    const bool held = b.ribbonStreams && b.ribbonStreams->gl == (engine.api() == gpu::GpuApi::OpenGL);
    for (int i = 0; i < GpuRibbonStreams::kBufferCount; ++i) t[i] = held ? b.ribbonStreams->target(i) : nullptr;
}

bool GroomSubSceneOverride::writeGpuRibbons(ItemBuffers& b, const DisplayGeometry& ribbons, MHWRender::MVertexBufferArray& streams,
                                            double* megabytes, double* ms) {
    const StrandLines* lines = ribbons.lines.get();
    gpu::LineFrames* engine = gpuLines_ ? gpuLines_->engine() : nullptr;
    if (!engine || !lines || !lines->strands || lines->strands->pointCount() == 0) return false;
    try {
        if (!writeGpuRibbons(*engine, b, ribbons, megabytes, ms)) return false;
    } catch (const std::exception& e) {
        // The device would not (a kernel, a buffer): the CPU builds the ribbons (and writes the
        // lines' frames) from now on.
        gpuLines_->fail(e.what());
        MGlobal::displayWarning(MString("GroomLab: the ribbons are built on the CPU (") + e.what() + ")");
        return false;
    }
    streams.addBuffer("positions", b.positions.get());
    if (b.lit) {
        streams.addBuffer("normals", b.normals.get());
        streams.addBuffer("tangents", b.tangents.get());
        streams.addBuffer("uvs", b.uvs.get());
    }
    if (b.vertexColors) streams.addBuffer("colors", b.colors.get());
    return true;
}

bool GroomSubSceneOverride::writeGpuRibbons(gpu::LineFrames& engine, ItemBuffers& b, const DisplayGeometry& ribbons, double* megabytes,
                                            double* ms) {
    const StrandLines& lines = *ribbons.lines;
    const StrandSet& s = *lines.strands;
    const auto count = static_cast<unsigned>(s.pointCount());
    const bool lit = lines.lit;
    // The colors the item's shader reads: unlit, one a strand (kind 1); lit, the strands' own
    // (kind 9; kind 2, Maya's graph as it is, takes the item's one color).
    const StrandColors* colors = lit && b.shaderKind != 9 ? nullptr : lines.colors.get();
    const bool gl = engine.api() == gpu::GpuApi::OpenGL;
    if (!lit && !colors) return false;  // (unlit ribbons take their strands' colors: the builder gives them)
    if (2ull * count > 0xffffffffull / 4) throw std::runtime_error("more ribbon vertices than 32-bit indices hold");
    // Buffers made now (for these counts, lit or not, colored or not): their triangles, uvs and
    // colors are written whatever the keys say.
    bool fresh = false;
    const GpuRibbonStreams* held = b.ribbonStreams.get();
    if (!held || held->gl != gl || held->device != engine.device() || held->count != count || held->lit != lit ||
        held->colors != (colors != nullptr)) {
        if (!held) {  // (the item's own buffers, Maya's, go first: the streams' take their place)
            b.positions.reset();
            b.normals.reset();
            b.tangents.reset();
            b.uvs.reset();
            b.colors.reset();
            b.indices.reset();
        }
        std::unique_ptr<MHWRender::MVertexBuffer>* vertices[7] = {&b.ribbonPoints, &b.ribbonUvs, &b.positions, &b.normals,
                                                                  &b.tangents,     &b.uvs,       &b.colors};
        b.ribbonStreams = gpuLines_->makeRibbonStreams(count, lit, colors != nullptr, b.ribbonStreams, vertices, &b.indices);
        fresh = true;
    }
    if (fresh) b.ribbonLayout = b.ribbonUvsKey = b.ribbonColorsKey = b.ribbonIdsKey = 0;
    b.bitangents.reset();
    b.lit = lit;
    b.vertexColors = colors != nullptr;
    b.coverage = false;
    b.layout = b.uvsKey = b.uvsIdsKey = b.colorsKey = 0;  // (the lines' keys: none here)
    b.ribbonWidthScale = ribbons.ribbonWidthScale;
    void* t[GpuRibbonStreams::kBufferCount] = {};
    gpuRibbonTargets(engine, b, t);
    for (int i = 0; i < GpuRibbonStreams::kBufferCount; ++i) {
        const bool used = i < 3 || i == GpuRibbonStreams::kIndices || (lit && i <= GpuRibbonStreams::kUvs) ||
                          (colors && i == GpuRibbonStreams::kColors);
        if (used && !t[i]) throw std::runtime_error("the ribbons' buffers have no names on the device");
    }

    double bytes = 0;
    Lap lap;
    // The lines' points into the streams (following the scalp on the GPU: carried there below).
    if (!follow_) {
        engine.write(t[GpuRibbonStreams::kPoints], s.positions().data(), 0, std::size_t{count} * 12);
        bytes += static_cast<double>(count) * 12;
    }
    ms[0] = lap();
    // The layout and root normals (as the lit lines'), when they changed.
    bytes += static_cast<double>(engine.setLayout(s.offsets().data(), s.strandCount(), lines.layoutKey()));
    ms[1] = lap();
    if (follow_) {
        double carried = 0;
        carryOnGpu(engine, b, t[GpuRibbonStreams::kPoints], lines.layoutKey(), &carried);
        bytes += carried * 1e6;
        ms[2] = lap();
    } else {
        thread_local std::vector<float> up;
        up.resize(s.strandCount() * 4);
        const std::uint64_t upKey = lines.writeRootNormals(up.data());
        ms[2] = lap();
        bytes += static_cast<double>(engine.setRootNormals(up.data(), upKey));
    }
    // The lines' uvs, for their widths: the widths alone, or the uvs whole (u with the strand's
    // number, which the lit ribbons' uvs take for the hair shading's strand variation, when the
    // strands' IDs changed; kNoLineWidth where the strands carry no widths).
    const std::uint64_t uvsKey = hashCombine(lines.layoutKey(), lines.widthsKey()) | 1u;
    const bool idsHeld = !lit || b.ribbonIdsKey == lines.idsKey();
    if (b.ribbonUvsKey != uvsKey || !idsHeld) {
        const float* widths = s.widths().size() == s.pointCount() ? s.widths().data() : nullptr;
        if (widths && idsHeld) {
            bytes += static_cast<double>(engine.writeWidths(t[GpuRibbonStreams::kLineUvs], widths));
        } else {
            thread_local std::vector<float> scratch;
            scratch.resize(std::size_t{count} * 2);
            lines.writeUvs(scratch.data(), 2);
            engine.write(t[GpuRibbonStreams::kLineUvs], scratch.data(), 0, std::size_t{count} * 8);
            bytes += static_cast<double>(count) * 8;
        }
        b.ribbonUvsKey = uvsKey;
        b.ribbonIdsKey = lit ? lines.idsKey() : 0;
    }
    // The triangles (and lit uvs: the lines' u with the strand's number) when the point counts
    // (or, lit, the strands' IDs) changed. (ribbonLayout stays the lines' layout key: following the
    // scalp, carryOnGpu checks the engine's layout by it.)
    if (b.ribbonLayout != lines.layoutKey() || lines.layoutKey() == 0 || !idsHeld) {
        engine.writeRibbonLayout(t[GpuRibbonStreams::kIndices], lit ? t[GpuRibbonStreams::kUvs] : nullptr,
                                 lit ? t[GpuRibbonStreams::kLineUvs] : nullptr);
        b.ribbonLayout = lines.layoutKey();
        b.ribbonColorsKey = 0;  // (colors keyed for other point counts)
    }
    // The colors, when they changed: lit, the strands' own a point, onto the vertices once; unlit,
    // one a strand, held by the engine (each run shades them).
    if (colors) {
        const std::uint64_t key = colors->key;
        if (lit && (key == 0 || key != b.ribbonColorsKey)) {
            thread_local std::vector<float> rgba;
            rgba.resize(std::size_t{count} * 4);
            colors->write(rgba.data());
            bytes += static_cast<double>(engine.setRibbonColors(rgba.data(), count, 0));
            engine.writeRibbonColors(t[GpuRibbonStreams::kColors]);
        } else if (!lit && !engine.ribbonColorsHeld(key)) {
            const std::size_t strands = s.strandCount();
            thread_local std::vector<float> rgba;
            rgba.assign(strands * 4, 1.0f);
            for (std::size_t i = 0; i < strands && i < colors->colors.size(); ++i) {
                for (int c = 0; c < 3; ++c) rgba[i * 4 + c] = colors->colors[i][c];
            }
            bytes += static_cast<double>(engine.setRibbonColors(rgba.data(), strands, key));
        }
        b.ribbonColorsKey = key;
    }
    ms[3] = lap();
    expandGpuRibbons(engine, b, t[GpuRibbonStreams::kPoints], t[GpuRibbonStreams::kLineUvs]);
    ms[4] = lap();
    ms[5] = 0;
    *megabytes = bytes / 1e6;
    return true;
}

void GroomSubSceneOverride::expandGpuRibbons(gpu::LineFrames& engine, ItemBuffers& b, void* points, void* lineUvs) {
    void* t[GpuRibbonStreams::kBufferCount] = {};
    gpuRibbonTargets(engine, b, t);
    gpu::LineFrames::RibbonTargets targets;
    targets.positions = t[GpuRibbonStreams::kPositions];
    targets.normals = b.lit ? t[GpuRibbonStreams::kNormals] : nullptr;
    targets.tangents = b.lit ? t[GpuRibbonStreams::kTangents] : nullptr;
    targets.colors = b.lit ? nullptr : t[GpuRibbonStreams::kColors];
    const Vec3f key = ribbonKeyLight();
    const float light[3] = {key.x, key.y, key.z};
    engine.runRibbons(points, lineUvs, b.ribbonWidthScale, b.lit, targets, light);
}

void GroomSubSceneOverride::updateHairGrid(const MHWRender::MFrameContext&) {
    const float strength = floatPlug(GroomShape::aDisplaySelfShadowing);
    // The hair shaders of the groom: its items' and the level of detail's copies.
    auto forEachHairShader = [&](const std::function<void(MHWRender::MShaderInstance&)>& fn) {
        for (auto& [name, b] : items_) {
            if (b.shader && b.hair) fn(*b.shader);
        }
        const auto base = items_.find(lodBase_.asChar());
        if (lodOn_ && base != items_.end() && base->second.hair) {
            for (MHWRender::MShaderInstance* shader : lodShaders_) {
                if (shader) fn(*shader);
            }
        }
    };
    if (!hairShading_ || !hairState_ || strength <= 0.0f) {
        if (hairState_) {
            hairState_->why = hairShading_ && strength <= 0.0f ? "displaySelfShadowing is 0" : "";
            hairState_->valid = false;  // (written again when asked for: the strands may move meanwhile)
        }
        forEachHairShader([&](MHWRender::MShaderInstance& s) { setHairGrid(s, nullptr, 0.0f); });
        return;
    }
    HairGroomState& grid = *hairState_;
    // The item drawing the strands with the hair shading: the lines (lit, or in their own colors), or the ribbons' lines.
    const MString* names[3] = {&kStrandsItem, &kColoredStrandsItem, &kRibbonsItem};
    const MString* drawn = nullptr;
    for (const MString* n : names) {
        const auto it = items_.find(n->asChar());
        if (it != items_.end() && it->second.shown && it->second.hair && it->second.linePoints > 0) {
            drawn = n;
            break;
        }
    }
    gpu::LineFrames* engine = gpuLines_ ? gpuLines_->engine() : nullptr;
    const StrandsPtr strands = shape_->handedStrands();
    // The cards view in the strands' place: the cards stand for them, so they keep the grid the
    // strands last made (without it they would light with no shadow at all and read far brighter
    // than the strands); remade once the strands draw again.
    const auto cards = items_.find(kCardsItem.asChar());
    if (!drawn && grid.valid && cards != items_.end() && cards->second.shown && cards->second.hair) {
        forEachHairShader([&](MHWRender::MShaderInstance& s) { setHairGrid(s, &grid, strength); });
        return;
    }
    if (!drawn || !engine || !strands) {
        grid.valid = false;
        grid.why = !drawn ? "no lit strands" : !engine ? (gpuLines_ ? gpuLines_->why() : std::string("no GPU")) : "no strands";
        forEachHairShader([&](MHWRender::MShaderInstance& s) { setHairGrid(s, nullptr, 0.0f); });
        return;
    }
    ItemBuffers& b = items_[drawn->asChar()];
    // The grid over the groom's bounds: cells a few strands apart at the render's density (the
    // strand spacing over the scalp), at least 16 and at most 192 along any axis, 4M in all.
    MBoundingBox box = shape_->boundingBox();
    const MPoint lo = box.min(), hi = box.max();
    const double margin = 0.02 * (hi - lo).length() + 1e-3;
    const double extent[3] = {hi.x - lo.x + 2 * margin, hi.y - lo.y + 2 * margin, hi.z - lo.z + 2 * margin};
    const double longest = std::max({extent[0], extent[1], extent[2]});
    const float density = std::clamp(shape_->evaluatedDensity(), 1e-3f, 1.0f);
    const MeshPtr scalp = shape_->evaluatedScalp();
    const double renderStrands = static_cast<double>(strands->strandCount()) / density;
    const double spacing = scalp && scalp->totalArea() > 0.0f && renderStrands > 0 ? std::sqrt(scalp->totalArea() / renderStrands) : 0.0;
    double cell = std::max({3.0 * spacing, longest / 192.0, std::cbrt(extent[0] * extent[1] * extent[2] / 4.0e6)});
    cell = std::min(cell, longest / 16.0);
    unsigned size[3];
    float origin[3];
    for (int k = 0; k < 3; ++k) {
        size[k] = static_cast<unsigned>(std::clamp(std::ceil(extent[k] / cell), 1.0, 256.0));
        origin[k] = static_cast<float>((k == 0 ? lo.x : k == 1 ? lo.y : lo.z) - margin);
    }
    // Written again when the strands moved this update (written, carried, a GPU preview's) or the grid moved.
    std::uint64_t key = hashFloat(hashFloat(hash64(size[0] * 73856093ull ^ size[1] * 19349663ull ^ size[2] * 83492791ull), static_cast<float>(cell)),
                                  origin[0] + 3.0f * origin[1] + 7.0f * origin[2]);
    key = hashCombine(key, reinterpret_cast<std::uintptr_t>(drawn));
    const DrawTimes& now = currentDraw();
    const bool moved = now.items.count(drawn->asChar()) || follow_ || gpuRunning();
    if (grid.valid && key == hairGridKey_ && !moved) {
        grid.why.clear();
        forEachHairShader([&](MHWRender::MShaderInstance& s) { setHairGrid(s, &grid, strength); });
        return;
    }
    // The points as the item draws them: the ribbons' lines, else the streams bound last.
    void* positions = nullptr;
    void* uvs = nullptr;
    const bool gl = engine->api() == gpu::GpuApi::OpenGL;
    if (b.ribbonStreams) {
        positions = b.ribbonStreams->target(GpuRibbonStreams::kPoints);
        uvs = b.ribbonStreams->target(GpuRibbonStreams::kLineUvs);
    } else if (!drawn->operator==(kRibbonsItem)) {
        const auto bound = boundStreams_.find(drawn->asChar());
        if (bound != boundStreams_.end()) {
            for (const auto& [name, buffer] : bound->second) {
                if (!buffer) continue;
                const MHWRender::MGeometry::Semantic semantic = buffer->descriptor().semantic();
                void* handle = gl ? glBufferName(buffer) : buffer->resourceHandle();
                if (semantic == MHWRender::MGeometry::kPosition) positions = handle;
                if (semantic == MHWRender::MGeometry::kTexture) uvs = handle;
            }
        }
    }
    MHWRender::MTexture* volume = grid.volumeFor(size);
    void* target = nullptr;
    if (volume) {
        void* handle = volume->resourceHandle();
        target = gl ? (handle ? reinterpret_cast<void*>(static_cast<std::uintptr_t>(*static_cast<const GLuint*>(handle))) : nullptr) : handle;
    }
    grid.valid = false;
    if (!positions || !uvs || !target) {
        grid.why = !positions || !uvs ? "the strands' buffers are not ones the GPU can read" : "no texture for the grid";
    } else {
        gpu::LineFrames::DensityGrid g;
        for (int k = 0; k < 3; ++k) {
            g.origin[k] = origin[k];
            g.size[k] = size[k];
            grid.origin[k] = origin[k];
        }
        g.cell = static_cast<float>(cell);
        grid.cell = g.cell;
        // A point of width w blocks (pi / 4) w x its piece of strand (the strands' mean segment,
        // from a sample of them: written whole or in part, the same strands give the same), at the
        // render's density.
        double length = 0.0;
        std::size_t segments = 0;
        const StrandSet& s = *strands;
        const std::size_t every = std::max<std::size_t>(1, s.strandCount() / 2048);
        for (std::size_t k = 0; k < s.strandCount(); k += every) {
            const std::size_t begin = s.strandBegin(k), n = s.strandPointCount(k);
            for (std::size_t i = 1; i < n; ++i) {
                const Vec3f d = s.positions()[begin + i] - s.positions()[begin + i - 1];
                length += std::sqrt(static_cast<double>(d.x) * d.x + static_cast<double>(d.y) * d.y + static_cast<double>(d.z) * d.z);
            }
            segments += n > 1 ? n - 1 : 0;
        }
        const double piece = segments ? length / static_cast<double>(segments) : cell;
        g.pieceWeight = static_cast<float>(0.785398 * piece / (cell * cell * cell) / density);
        g.defaultWidth = 0.01f;
        g.occlusionReach = 6.0f;
        try {
            engine->runDensity(positions, uvs, b.linePoints, g, target);
            grid.valid = true;
            grid.why.clear();
            ++grid.serial;
            hairGridKey_ = key;
        } catch (const std::exception& e) {
            grid.why = e.what();
        }
    }
    forEachHairShader([&](MHWRender::MShaderInstance& s) { setHairGrid(s, &grid, strength); });
}

bool GroomSubSceneOverride::followDrawable(const DisplaySettings& s, const StrandSet* strands, bool stroke) const {
    // The lit lines (their frames by the GPU) or ribbons (made by the GPU) of the strands in one
    // color or their own; not a stroke's dimmed strands, a Scatter's root dots or the cards.
    if (stroke || s.cardsView || s.colorMode != kSolidColors || shape_->showsRoots()) return false;
    if (!s.ribbons && !s.lighting) return false;  // flat lines: the CPU writes their points
    // Feathers' vanes lean their frames (VaneNormals), which the GPU's root normals do not.
    return !(strands && strands->strandChannel(kVaneNormalChannels[0]));
}

void GroomSubSceneOverride::carryOnGpu(gpu::LineFrames& engine, ItemBuffers& b, void* points, std::uint64_t layout,
                                       double* megabytes) {
    const FollowCarry& f = *follow_;
    const StrandSet& s = *f.rest();
    const ScalpMesh& from = *f.from();
    const ScalpMesh& to = *f.to();
    if (!points) throw std::runtime_error("no points to carry the strands into");
    if (s.strandCount() != followRoots_.size() || from.pointCount() != to.pointCount() || from.vertexNormals().size() != from.pointCount() ||
        to.vertexNormals().size() != to.pointCount() || s.pointCount() == 0 || from.pointCount() == 0) {
        throw std::runtime_error("the strands at rest and the scalps do not match");
    }
    double bytes = static_cast<double>(engine.setLayout(s.offsets().data(), s.strandCount(), layout));
    bytes += static_cast<double>(engine.setFollowRest(&s.positions()[0].x, followRestVersion_));
    bytes += static_cast<double>(engine.setFollowRoots(followRoots_.data(), followRootsKey_));
    bytes += static_cast<double>(engine.setFollowScalp(false, &from.points()[0].x, &from.vertexNormals()[0].x, from.pointCount(),
                                                       from.contentHash() | 1u));
    bytes += static_cast<double>(engine.setFollowScalp(true, &to.points()[0].x, &to.vertexNormals()[0].x, to.pointCount(),
                                                       to.contentHash() | 1u));
    engine.runFollow(points);
    b.followRest = followRestVersion_;
    b.followTo = to.contentHash() | 1u;
    *megabytes = bytes / 1e6;
}

void GroomSubSceneOverride::carryFollowed(MHWRender::MSubSceneContainer& container) {
    gpu::LineFrames* engine = gpuLines_ ? gpuLines_->engine() : nullptr;
    if (!follow_ || !engine) return;
    const std::uint64_t to = follow_->to()->contentHash() | 1u;
    for (const MString* name : {&kStrandsItem, &kRibbonsItem}) {
        auto it = items_.find(name->asChar());
        MHWRender::MRenderItem* item = container.find(*name);
        if (it == items_.end() || !item || !it->second.shown) continue;
        ItemBuffers& b = it->second;
        // Only what the GPU carried for these strands at rest (the builder's write), for another scalp.
        if (b.followRest != followRestVersion_ || b.followTo == to || !b.indices) continue;
        DrawTimes::Item& timed = currentDraw().items[name->asChar()];
        Lap lap;
        double megabytes = 0;
        MHWRender::MVertexBufferArray streams;
        try {
            if (name == &kRibbonsItem) {
                void* t[GpuRibbonStreams::kBufferCount] = {};
                gpuRibbonTargets(*engine, b, t);
                if (!t[GpuRibbonStreams::kPoints] || !t[GpuRibbonStreams::kLineUvs]) throw std::runtime_error("the ribbons' lines are gone");
                carryOnGpu(*engine, b, t[GpuRibbonStreams::kPoints], b.ribbonLayout, &megabytes);
                expandGpuRibbons(*engine, b, t[GpuRibbonStreams::kPoints], t[GpuRibbonStreams::kLineUvs]);
                streams.addBuffer("positions", b.positions.get());
                if (b.lit) {
                    streams.addBuffer("normals", b.normals.get());
                    streams.addBuffer("tangents", b.tangents.get());
                    streams.addBuffer("uvs", b.uvs.get());
                }
                if (b.vertexColors) streams.addBuffer("colors", b.colors.get());
            } else {
                void* targets[3] = {};
                MHWRender::MVertexBuffer* own[3] = {b.positions.get(), b.normals.get(), b.tangents.get()};
                for (int i = 0; i < 3; ++i) {
                    targets[i] = b.gpuStreams ? static_cast<void*>(b.gpuStreams->buffers[i])
                                              : engine->api() == gpu::GpuApi::OpenGL ? glBufferName(own[i]) : nullptr;
                    if (!targets[i]) throw std::runtime_error("the lines' buffers have no names on the device");
                }
                carryOnGpu(*engine, b, targets[0], b.layout, &megabytes);
                engine->run(targets[0], targets[1], targets[2]);
                addStreams(streams, b);
            }
        } catch (const std::exception& e) {
            gpuLines_->fail(e.what());
            MGlobal::displayWarning(MString("GroomLab: the strands following the scalp are carried on the CPU (") + e.what() + ")");
            followBroken_ = true;
            return;
        }
        timed.writeMs += lap();
        timed.megabytes += megabytes;
        timed.gpuMegabytes += megabytes;
        timed.gpuFrames = true;
        timed.follow = true;
        const MBoundingBox bounds = shape_->boundingBox();
        bindGeometry(*item, streams, *b.indices, &bounds);
        ownBuffersBound(*name);
        timed.bindMs += lap();
    }
}

bool gpuPreviewDrawn(const GroomShape* shape, std::uint64_t version) {
    for (auto [at, end] = drawnOverrides().equal_range(shape); at != end; ++at) {
        if (version != 0 && at->second->gpuShown_ == version) return true;
    }
    return false;
}

void GroomSubSceneOverride::ownBuffersBound(const MString& name) {
    if (name != gpuItem_) return;
    gpuBound_ = false;
    gpuBoundsSet_ = false;  // its own buffers with the CPU's bounds
    gpuStale_ = true;
}

bool GroomSubSceneOverride::gpuRunning() const {
    const GpuPreviewRequest* request = gpuPreviewRequest(shape_);
    return (request && request->active()) || (shape_->hasPreview() && !shape_->gpuStroke().empty());
}

void GroomSubSceneOverride::rebindOwnBuffers(MHWRender::MSubSceneContainer& container, const MString& name) {
    // The item draws its own buffers again (as the CPU last wrote them), with the CPU's bounds:
    // the preview's are no longer referenced and may go.
    auto it = items_.find(name.asChar());
    MHWRender::MRenderItem* item = container.find(name);
    if (item && it != items_.end() && it->second.positions && it->second.indices) {
        const ItemBuffers& b = it->second;
        MHWRender::MVertexBufferArray vertexBuffers;
        addStreams(vertexBuffers, b);
        const MBoundingBox bounds = shape_->boundingBox();
        bindGeometry(*item, vertexBuffers, *b.indices, &bounds);
    }
    ownBuffersBound(name);
}

void GroomSubSceneOverride::applyGpuPreview(MHWRender::MSubSceneContainer& container, const StrandsPtr& strands) {
    // The item the strands draw as lines: the lit (or flat) strands, a stroke's dimmed ones, the
    // colored ones (their positions alone: the colors stay), else ribbons the GPU makes from lines
    // (the run writes the lines, the ribbons are made from them: expandGpuRibbons). Cards, and
    // ribbons built on the CPU, are the CPU's.
    if (gpuPreviewEnabled()) prepareGpuKernels();  // (OpenGL: compiled before the first drag or stroke)
    const MString* name = nullptr;
    bool cpuRibbons = false;
    for (const MString* candidate : {&kStrandsItem, &kDimStrandsItem, &kColoredStrandsItem, &kRibbonsItem}) {
        auto it = items_.find(candidate->asChar());
        if (it != items_.end() && it->second.shown && it->second.positions && container.find(*candidate)) {
            if (candidate == &kRibbonsItem && !it->second.ribbonPoints) {
                cpuRibbons = true;
                continue;
            }
            name = candidate;
            break;
        }
    }
    ItemBuffers* b = name ? &items_[name->asChar()] : nullptr;
    const bool ribbons = name == &kRibbonsItem;
    MHWRender::MVertexBuffer* points = !b ? nullptr : ribbons ? b->ribbonPoints.get() : b->positions.get();
    const bool lines = b && (ribbons || (b->shaderKind != 5 && b->shaderKind != 6 && b->shaderKind != 7 && b->shaderKind != 8));
    // (The lines' points: a custom handle's vertexCount is not theirs under DirectX 11.)
    const unsigned pointCount = !b ? 0u : ribbons ? (b->ribbonStreams ? b->ribbonStreams->count : 0u) : b->positions->vertexCount();
    const bool drawable = lines && strands && pointCount == strands->pointCount() && b->indices;
    gpuPreview_->setDrawable(drawable, cpuRibbons && !b ? "the ribbons are built on the CPU"
                                       : !b         ? "the strands are not drawn"
                                       : !lines     ? "the strands are not drawn as lines"
                                                    : "the strands drawn are not the graph's output");
    const GpuPreviewRequest* request = gpuPreviewRequest(shape_);
    const bool dragging = request && request->active();
    // A live brush stroke: once its press is drawn (the CPU's), the GPU takes its next dabs where
    // it can (the shape's evaluation of each stops at Interpolate); after it, the next one starts
    // afresh.
    if (!shape_->hasPreview()) gpuPreview_->endStroke(*shape_);
    const bool stroking = shape_->hasPreview() && !shape_->gpuStroke().empty();
    if (!dragging && !stroking) {
        gpuShown_ = 0;
        if (shape_->hasPreview() && !gpuPreview_->strokeArmed() && gpuPreviewEnabled()) gpuPreview_->armStroke(*shape_);
        // The item draws its own buffers again: the preview's can go.
        if (gpuBound_) rebindOwnBuffers(container, gpuItem_);
        if (!gpuBound_) gpuPreview_->releaseDisplay();
        return;
    }
    // Another item than the one drawing the preview's buffers: that one draws its own again first
    // (the preview's buffers may be made anew for this one).
    if (gpuBound_ && (!name || *name != gpuItem_)) rebindOwnBuffers(container, gpuItem_);
    const std::uint64_t version = dragging ? request->version : gpuPreview_->strokeDabs() | (std::uint64_t{1} << 63);
    const std::uint64_t strandsVersion = shape_->strandsVersion();
    // Again for new values (a dab), new strands, or the item's own buffers bound or written since.
    if (version == gpuDrawn_ && strandsVersion == gpuStrandsDrawn_ && !gpuStale_) return;
    gpuDrawn_ = version;
    gpuStrandsDrawn_ = strandsVersion;
    gpuStale_ = false;
    if (!drawable) return;
    Lap lap;
    // (Ribbons: their lines alone, the frames the ribbons' own kernel's.)
    const bool lit = !ribbons && b->lit && b->normals && b->tangents;
    GpuItemBuffers item{points, lit ? b->normals.get() : nullptr, lit ? b->tangents.get() : nullptr, pointCount};
    // With width as coverage the lines' uvs too: a Width the GPU runs writes its widths there (the
    // ribbons' lines always: their widths).
    item.uvs = ribbons ? b->ribbonUvs.get()
               : b->coverage && b->uvs && b->uvs->vertexCount() == item.vertexCount ? b->uvs.get()
                                                                                    : nullptr;
    if (ribbons && b->ribbonStreams && b->ribbonStreams->gl) {  // (custom handles: named by the streams)
        item.glPositions = b->ribbonStreams->target(GpuRibbonStreams::kPoints);
        item.glUvs = b->ribbonStreams->target(GpuRibbonStreams::kLineUvs);
    }
    GpuItemBuffers draw;
    const bool ran = dragging ? gpuPreview_->apply(*shape_, *request, item, &draw) : gpuPreview_->applyStroke(*shape_, item, &draw);
    gpuShown_ = ran && dragging ? version : 0;
    if (!ran) return;
    gpuWrote_ = true;
    gpuWroteItem_ = gpuWroteItem_ || gpuPreview_->wroteItemBuffers();
    // The item's own uvs written in place (OpenGL): they no longer hold the CPU's widths.
    if (gpuPreview_->wroteItemUvs()) b->uvsKey = b->ribbonUvsKey = b->ribbonIdsKey = 0;
    gpuItem_ = *name;
    if (ribbons) {
        // The ribbons from the run's lines: the preview's own buffers (DirectX 11; its uvs where a
        // Width ran, else the item's lines' own), or the item's lines written in place (OpenGL).
        gpu::LineFrames* engine = gpuLines_ ? gpuLines_->engine() : nullptr;
        void* own[GpuRibbonStreams::kBufferCount] = {};
        if (engine) gpuRibbonTargets(*engine, *b, own);
        void* runPoints = draw.positions ? gpuPreview_->displayBuffer(0) : own[GpuRibbonStreams::kPoints];
        void* runUvs = draw.positions && draw.uvs ? gpuPreview_->displayBuffer(3) : own[GpuRibbonStreams::kLineUvs];
        try {
            if (!engine || !runPoints || !runUvs) throw std::runtime_error("no lines to make the ribbons from");
            expandGpuRibbons(*engine, *b, runPoints, runUvs);
        } catch (const std::exception& e) {
            gpuLines_->fail(e.what());
            MGlobal::displayWarning(MString("GroomLab: the ribbons are built on the CPU (") + e.what() + ")");
            builder_.invalidate(DisplayPart::Strands);  // (built again, on the CPU)
            return;
        }
        gpuWroteItem_ = true;  // (the ribbons' own buffers: written whole from the CPU's once it ends)
    }
    DrawTimes::Item& timed = currentDraw().items["gpuPreview"];
    timed.writeMs = lap();
    MHWRender::MRenderItem* renderItem = container.find(*name);
    if (!renderItem) return;
    // The item's bounds for the drag: the CPU's grown by the farthest the drag can push a point
    // (lookBounds), so the viewport culls none of the hair it moved out.
    MBoundingBox bounds = shape_->boundingBox();
    const gpu::LookBounds& reach = gpuPreview_->bounds();
    if (reach.valid) {
        bounds.expand(MPoint(reach.lo.x, reach.lo.y, reach.lo.z));
        bounds.expand(MPoint(reach.hi.x, reach.hi.y, reach.hi.z));
    }
    // The preview's own buffers (DirectX 11), or the item's own written in place (OpenGL: GPU-only,
    // bound again for the bounds alone; a copy into DirectX 11's staged buffers is left bound as
    // it is, binding it again could have Maya upload its system-memory copy over the GPU's).
    // (Ribbons draw their own buffers, the kernels' writes: bound again for the bounds alone.)
    if (ribbons) draw = GpuItemBuffers{};
    if (!ribbons && !draw.positions && !gpuPreview_->writesItemInPlace()) return;
    // Bound already with bounds holding these: nothing to do (binding costs Viewport 2.0 more than
    // the kernels do). Growing, the bounds keep what they held (a drag back and forth binds once).
    // (Buffers made anew are always bound: the item must never draw the ones they replaced.)
    const bool bound = draw.positions ? gpuBound_ && !draw.remade : gpuBoundsSet_;
    if (bound && gpuBounds_.contains(bounds.min()) && gpuBounds_.contains(bounds.max())) return;
    const gpu::LookBounds& roomy = gpuPreview_->roomyBounds();
    if (roomy.valid) {
        bounds.expand(MPoint(roomy.lo.x, roomy.lo.y, roomy.lo.z));
        bounds.expand(MPoint(roomy.hi.x, roomy.hi.y, roomy.hi.z));
    }
    if (bound) {
        bounds.expand(gpuBounds_.min());
        bounds.expand(gpuBounds_.max());
    }
    MHWRender::MVertexBufferArray vertexBuffers;
    addStreams(vertexBuffers, *b, draw.positions, draw.positions ? draw.normals : nullptr, draw.positions ? draw.tangents : nullptr,
               draw.positions ? draw.uvs : nullptr);
    bindGeometry(*renderItem, vertexBuffers, *b->indices, &bounds);
    if (draw.positions) gpuBound_ = true;
    gpuBounds_ = bounds;
    gpuBoundsSet_ = true;
    timed.bindMs = lap();
}
namespace {

// Ribbons the GPU made from the lines (writeGpuRibbons) against the CPU's strandRibbons, vertex by
// vertex (the CPU's leave strands of one point out): the largest differences.
nlohmann::json ribbonsCheck(gpu::LineFrames& frames, GroomShape& shape, const void* const targets[8], bool lit, float widthScale,
                            unsigned pointsDrawn) {
    nlohmann::json r;
    const StrandsPtr strands = shape.evaluatedStrands();
    const std::size_t n = strands ? strands->pointCount() : 0;
    if (!n || pointsDrawn != n) {
        r["why"] = "the ribbons drawn are not the evaluated strands'";
        return r;
    }
    const std::vector<std::array<float, 3>> white(strands->strandCount(), std::array<float, 3>{1.0f, 1.0f, 1.0f});
    const DisplayGeometry cpu = strandRibbons(*strands, shape.evaluatedScalp().get(), white, widthScale, lit);
    const std::vector<float> positions = frames.read(const_cast<void*>(targets[GpuRibbonStreams::kPositions]), n * 6);
    const std::vector<float> normals = lit ? frames.read(const_cast<void*>(targets[GpuRibbonStreams::kNormals]), n * 6) : std::vector<float>();
    const std::vector<float> tangents = lit ? frames.read(const_cast<void*>(targets[GpuRibbonStreams::kTangents]), n * 6) : std::vector<float>();
    double worst[3] = {};
    std::size_t v = 0;
    for (std::size_t i = 0; i < strands->strandCount(); ++i) {
        const std::size_t pts = strands->strandPointCount(i), begin = strands->strandBegin(i);
        if (pts < 2) continue;
        for (std::size_t k = 0; k < 2 * pts; ++k, ++v) {
            const std::size_t g = 2 * begin + k;
            for (int c = 0; c < 3; ++c) {
                worst[0] = std::max(worst[0], std::abs(static_cast<double>(positions[g * 3 + c]) - cpu.positions[v * 3 + c]));
                if (!lit) continue;
                worst[1] = std::max(worst[1], std::abs(static_cast<double>(normals[g * 3 + c]) - cpu.normals[v * 3 + c]));
                worst[2] = std::max(worst[2], std::abs(static_cast<double>(tangents[g * 3 + c]) - cpu.tangents[v * 3 + c]));
            }
        }
    }
    r["positions"] = worst[0];
    if (lit) {
        r["normals"] = worst[1];
        r["tangents"] = worst[2];
    }
    r["points"] = n;
    r["lit"] = lit;
    return r;
}

}  // namespace

std::string gpuFramesCheckJson(const GroomShape* shape) {
    nlohmann::json j;
    // Ribbons the GPU makes, where an override of the shape draws them.
    for (auto [at, end] = drawnOverrides().equal_range(shape); at != end; ++at) {
        GroomSubSceneOverride& o = *at->second;
        auto item = o.items_.find(kRibbonsItem.asChar());
        if (item == o.items_.end() || !item->second.shown) continue;
        gpu::LineFrames* frames = o.gpuLines_ ? o.gpuLines_->engine() : nullptr;
        if (!frames || !item->second.ribbonPoints) {
            j["ribbons"] = {{"why", "built on the CPU (" + (o.gpuLines_ ? o.gpuLines_->why() : std::string("no GPU lines")) + ")"}};
            break;
        }
        void* targets[GpuRibbonStreams::kBufferCount] = {};
        GroomSubSceneOverride::gpuRibbonTargets(*frames, item->second, targets);
        j["ribbons"] = ribbonsCheck(*frames, *o.shape_, targets, item->second.lit, item->second.ribbonWidthScale,
                                    item->second.ribbonStreams ? item->second.ribbonStreams->count : 0u);
        break;
    }
    // The override drawing lit strands (of the shape's: one a viewport, say), else the first.
    GroomSubSceneOverride* drawing = nullptr;
    for (auto [at, end] = drawnOverrides().equal_range(shape); at != end; ++at) {
        auto item = at->second->items_.find(kStrandsItem.asChar());
        if (!drawing || (item != at->second->items_.end() && item->second.shown && item->second.lit)) drawing = at->second;
    }
    if (!drawing) {
        j["why"] = "not drawn";
        return j.dump();
    }
    GroomSubSceneOverride& o = *drawing;
    j["overrides"] = drawnOverrides().count(shape);
    auto it = o.items_.find(kStrandsItem.asChar());
    if (it == o.items_.end() || !it->second.shown || !it->second.lit || !it->second.positions || !it->second.uvs) {
        j["why"] = it == o.items_.end() ? "no strands item" : !it->second.shown ? "the strands item is not shown"
                 : !it->second.lit ? "the strands are not lit lines" : "the strands have no points or uvs";
        return j.dump();
    }
    GroomSubSceneOverride::ItemBuffers& b = it->second;
    gpu::LineFrames* frames = o.gpuLines_ ? o.gpuLines_->engine() : nullptr;
    j["why"] = o.gpuLines_ ? o.gpuLines_->why() : std::string("no GPU lines");
    j["gpuFrames"] = lastDraw().items.count(kStrandsItem.asChar()) ? lastDraw().items[kStrandsItem.asChar()].gpuFrames : false;
    if (!frames) return j.dump();
    j["api"] = gpu::apiName(frames->api());
    const StrandsPtr strands = o.shape_->evaluatedStrands();
    const std::size_t n = strands ? strands->pointCount() : 0;
    if (!n || b.positions->vertexCount() != n) {
        j["why"] = "the strands drawn are not the evaluated ones (" + std::to_string(b.positions->vertexCount()) + " points drawn, " + std::to_string(n) + " evaluated)";
        return j.dump();
    }
    MHWRender::MVertexBuffer* own[4] = {b.positions.get(), b.normals.get(), b.tangents.get(), b.uvs.get()};
    void* targets[4] = {};
    for (int i = 0; i < 4; ++i) {
        if (b.gpuStreams) {
            targets[i] = b.gpuStreams->buffers[i];
        } else {
            targets[i] = frames->api() == gpu::GpuApi::OpenGL ? glBufferName(own[i]) : (own[i] ? own[i]->resourceHandle() : nullptr);
        }
    }
    const StrandLines cpu(strands, o.shape_->evaluatedScalp(), true, nullptr);
    // Whether the uvs are held as the strands' (their keys).
    j["uvsHeld"] = b.uvsKey == (hashCombine(cpu.layoutKey(), cpu.widthsKey()) | 1u) && b.uvsIdsKey == cpu.idsKey();
    std::vector<float> want[4] = {std::vector<float>(n * 3), std::vector<float>(n * 3), std::vector<float>(n * 3), std::vector<float>(n * 2)};
    cpu.writePositions(want[0].data());
    cpu.writeFrames(want[1].data(), want[2].data());
    cpu.writeUvs(want[3].data(), 2);
    const char* names[4] = {"positions", "normals", "tangents", "u"};
    double widths = 0;
    for (int i = 0; i < 4; ++i) {
        if (!targets[i]) continue;
        const std::vector<float> got = frames->read(targets[i], want[i].size());
        double worst = 0;
        for (std::size_t k = 0; k < want[i].size(); ++k) {
            const double d = std::abs(static_cast<double>(got[k]) - want[i][k]);
            if (i == 3 && (k & 1)) {
                widths = std::max(widths, d);
            } else {
                worst = std::max(worst, d);
            }
        }
        j[names[i]] = worst;
    }
    j["widths"] = widths;
    // The strands' own colors (kind 9), where the item draws them.
    if (b.vertexColors && b.colors && b.colors->vertexCount() == n) {
        const std::shared_ptr<const StrandColors> look = strandLookColors(strands, !o.hairShading_);
        void* colors = frames->api() == gpu::GpuApi::OpenGL ? glBufferName(b.colors.get()) : b.colors->resourceHandle();
        if (look && colors) {
            const std::vector<float> want = look->rgba(), got = frames->read(colors, n * 4);
            double worst = 0;
            for (std::size_t k = 0; k < want.size(); ++k) worst = std::max(worst, std::abs(static_cast<double>(got[k]) - want[k]));
            j["colors"] = worst;
        }
    }
    j["points"] = n;
    return j.dump();
}

std::string hairGridJson(const GroomShape* shape) {
    nlohmann::json j = nlohmann::json::object();
    auto range = drawnOverrides().equal_range(shape);
    for (auto it = range.first; it != range.second; ++it) {
        const GroomSubSceneOverride& o = *it->second;
        j["hairShading"] = o.hairShading_;
        if (!o.hairState_) break;
        const HairGroomState& g = *o.hairState_;
        j["valid"] = g.valid;
        j["why"] = g.why;
        j["size"] = {g.size[0], g.size[1], g.size[2]};
        j["cell"] = g.cell;
        j["origin"] = {g.origin[0], g.origin[1], g.origin[2]};
        j["serial"] = g.serial;
        j["ignoredLights"] = g.ignoredLights.size();
        gpu::LineFrames* engine = o.gpuLines_ ? o.gpuLines_->engine() : nullptr;
        j["gpuMs"] = engine ? engine->gpuMs(true) : -1.0;
        break;
    }
    return j.dump();
}
}  // namespace groomlab::maya
