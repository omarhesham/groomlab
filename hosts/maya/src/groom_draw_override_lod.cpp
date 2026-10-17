// SPDX-License-Identifier: Apache-2.0
// The draw override's screen-space level of detail (strand_lod.h; displayClumpLod).

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

#include <maya/MBoundingBox.h>
#include <maya/MFnCamera.h>
#include <maya/MFrameContext.h>
#include <maya/MGlobal.h>
#include <maya/MDagPath.h>
#include <maya/MHWGeometry.h>
#include <maya/MShaderManager.h>
#include <maya/MViewport2Renderer.h>

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

// ---- Screen-space level of detail (strand_lod.h) ---------------------------------------------

namespace {

// Slots: the classes (0 to kLodClassCount - 1), the blend item, the blendable segments' opaque item.
constexpr std::size_t kLodBlendSlot = kLodClassCount, kLodBlendOpaqueSlot = kLodClassCount + 1;
MString lodItemName(std::size_t slot) {
    if (slot == kLodBlendSlot) return kLodBlendItem;
    if (slot == kLodBlendOpaqueSlot) return kLodBlendOpaqueItem;
    MString name = kLodItemPrefix;
    name += static_cast<int>(slot);
    return name;
}

}  // namespace

bool GroomSubSceneOverride::lodWanted() const {
    if (!lodLines_[0] && !lodLines_[1]) return false;
    return widthCoverage_ && displayFlag(GroomShape::aDisplayClumpLod);
}

namespace {

// The point Maya's tumble tool turns the camera about (tumbleCtx -localTumble: 1, the default, its
// center of interest; 0 its tumble pivot, which "tumble on recent selection" sets; 2 its own axis
// offset by the tumble pivot), asked of the tool at most once a second (interactive sessions alone:
// in batch there is no tool, and the center of interest stands).
int tumbleMode() {
    static int mode = 1;
    static std::chrono::steady_clock::time_point asked;
    static bool everAsked = false;
    const auto now = std::chrono::steady_clock::now();
    if (MGlobal::mayaState() == MGlobal::kInteractive && (!everAsked || now - asked > std::chrono::seconds(1))) {
        everAsked = true;
        asked = now;
        int exists = 0, value = 1;
        if (MGlobal::executeCommand("contextInfo -exists tumbleContext", exists, false, false) == MS::kSuccess && exists &&
            MGlobal::executeCommand("tumbleCtx -q -localTumble tumbleContext", value, false, false) == MS::kSuccess) {
            mode = value;
        }
    }
    return mode;
}

}  // namespace

StrandLodView GroomSubSceneOverride::lodViewOf(const MHWRender::MFrameContext& context) const {
    // This frame's camera: its eye, its view direction, the pixels a unit spans at clip w 1 (half
    // the viewport's height times the projection's [1][1]: display.h linePixelScale, as the
    // coverage's shaders take it), orthographic or not.
    const MMatrix inverse = context.getMatrix(MHWRender::MFrameContext::kViewInverseMtx);
    const MMatrix projection = context.getMatrix(MHWRender::MFrameContext::kProjectionMtx);
    int ox = 0, oy = 0, width = 0, height = 0;
    context.getViewportDimensions(ox, oy, width, height);
    const Vec3f eye(static_cast<float>(inverse(3, 0)), static_cast<float>(inverse(3, 1)), static_cast<float>(inverse(3, 2)));
    const Vec3f forward(-static_cast<float>(inverse(2, 0)), -static_cast<float>(inverse(2, 1)), -static_cast<float>(inverse(2, 2)));
    const bool orthographic = std::fabs(projection(3, 3) - 1.0) < 1e-6 && std::fabs(projection(2, 3)) < 1e-6;
    const float pixelScale = linePixelScale(static_cast<float>(projection(1, 1)), static_cast<float>(std::max(height, 1)));
    // Per camera and viewport size (a panel, a playblast of its camera at another size), each
    // with its level's hysteresis.
    MStatus status;
    const MDagPath cameraPath = context.getCurrentCameraPath(&status);
    std::string key = status == MS::kSuccess ? std::string(cameraPath.fullPathName().asChar()) : std::string("?");
    key += "|" + std::to_string(width) + "x" + std::to_string(height);
    // The point an orbit turns the camera about (tumbleMode): the view keeps what an orbit about it
    // at this distance can show (strand_lod.h), so tumbling changes nothing. Without a camera (or
    // its center of interest unusable) the strands' own center.
    Vec3f pivot = lod_.center();
    if (status == MS::kSuccess && !orthographic) {
        MStatus cameraStatus;
        MFnCamera fn(cameraPath, &cameraStatus);
        if (cameraStatus == MS::kSuccess) {
            const int mode = tumbleMode();
            if (mode == 1) {
                const double coi = fn.centerOfInterest(&cameraStatus);
                if (cameraStatus == MS::kSuccess && coi > 0.0 && std::isfinite(coi)) pivot = eye + forward.normalized() * static_cast<float>(coi);
            } else {
                const MPoint tp = fn.tumblePivot(&cameraStatus);
                if (cameraStatus == MS::kSuccess) {
                    const Vec3f p(static_cast<float>(tp.x), static_cast<float>(tp.y), static_cast<float>(tp.z));
                    if (mode == 0) {
                        pivot = p;
                    } else {  // (the camera's own axes, offset by the tumble pivot in them)
                        const auto row = [&](int r) {
                            return Vec3f(static_cast<float>(inverse(r, 0)), static_cast<float>(inverse(r, 1)), static_cast<float>(inverse(r, 2)));
                        };
                        pivot = eye + row(0) * p.x + row(1) * p.y + row(2) * p.z;
                    }
                }
            }
        }
    }
    const auto now = std::chrono::steady_clock::now();
    LodCamera& camera = lodCameras_[key];
    camera.view = lodView(eye, forward, pixelScale, orthographic, lod_.center(), pivot, camera.view);
    camera.seen = now;
    // The finest of the cameras seen in the last two seconds (several panels draw the same items).
    StrandLodView finest = camera.view;
    for (const auto& [name, other] : lodCameras_) {
        if (now - other.seen > std::chrono::seconds(2)) continue;
        const StrandLodView& v = other.view;
        if (v.level == kNoLodLevel) {
            finest = v;  // (one that cannot thin: none)
        } else if (finest.level != kNoLodLevel && v.pixelsAt(0.0f) > finest.pixelsAt(0.0f)) {
            finest = v;
        }
    }
    return finest;
}

void GroomSubSceneOverride::dropLodImpostors(MHWRender::MSubSceneContainer& container) {
    auto it = items_.find(kLodImpostorsItem.asChar());
    if (it == items_.end()) return;
    if (container.find(kLodImpostorsItem)) container.remove(kLodImpostorsItem);
    ownBuffersBound(kLodImpostorsItem);
    boundStreams_.erase(kLodImpostorsItem.asChar());
    releaseGeometry(it->second);
    if (it->second.shader) {
        const MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
        if (const MHWRender::MShaderManager* shaders = renderer ? renderer->getShaderManager() : nullptr) shaders->releaseShader(it->second.shader);
    }
    items_.erase(it);
    lodImpostorKey_ = lodImpostorColors_ = 0;
}

void GroomSubSceneOverride::hideLod(MHWRender::MSubSceneContainer& container) {
    const MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
    const MHWRender::MShaderManager* shaders = renderer ? renderer->getShaderManager() : nullptr;
    for (std::size_t slot = 0; slot < lodShaders_.size(); ++slot) {
        if (container.find(lodItemName(slot))) container.remove(lodItemName(slot));
        if (lodShaders_[slot] && shaders) shaders->releaseShader(lodShaders_[slot]);
        lodShaders_[slot] = nullptr;
    }
    for (auto& indices : lodIndices_) indices.reset();
    lodSlotKeys_ = {};
    lodBound_ = {};
    dropLodImpostors(container);
    // The strands' own item draws again (where it holds strands to draw).
    if (lodOn_ && lodBase_.length() > 0) {
        auto it = items_.find(lodBase_.asChar());
        MHWRender::MRenderItem* base = container.find(lodBase_);
        if (base && it != items_.end() && it->second.shown) base->enable(true);
        if (it != items_.end() && it->second.shader && it->second.coverage) it->second.shader->setParameter("lineBlend", 0.0f);
    }
    lodOn_ = false;
    lodKey_ = lodImpostorKey_ = lodImpostorColors_ = 0;
    lastLod().on = false;
    lastLod().items = 0;
}

void GroomSubSceneOverride::updateLod(MHWRender::MSubSceneContainer& container, const MHWRender::MFrameContext& context) {
    Lap lap;
    LodStats& stats = lastLod();
    lodAntiAliased_ = context.getPostEffectEnabled(MHWRender::MFrameContext::kAntiAliasing);  // (asked for again only when it changes)
    // The item the classes stand in for: the strands' (lit or flat), else the colored strands', where
    // it draws strand lines with width as coverage.
    int which = -1;
    for (int w = 0; w < 2 && which < 0; ++w) {
        const MString& name = w == 0 ? kStrandsItem : kColoredStrandsItem;
        const auto it = items_.find(name.asChar());
        if (lodLines_[w] && it != items_.end() && it->second.shown && it->second.coverage && it->second.shader && container.find(name)) {
            which = w;
        }
    }
    if (which < 0 || !lodWanted()) {
        hideLod(container);
        lodDrawn_ = StrandLodView{};
        return;
    }
    const MString& baseName = which == 0 ? kStrandsItem : kColoredStrandsItem;
    if (lodOn_ && lodBase_ != baseName) hideLod(container);
    ItemBuffers& base = items_[baseName.asChar()];
    const StrandLines& lines = *lodLines_[which];
    lod_.setStrands(lines.strands, "clumpId");
    const StrandLodView view = lodViewOf(context);
    lodDrawn_ = view;
    StrandLodSettings settings;
    settings.error = floatPlug(GroomShape::aClumpLodError);
    settings.widthScale = floatPlug(GroomShape::aRibbonWidthScale);
    settings.density = displayFlag(GroomShape::aDisplayCoverageDensity) ? shape_->evaluatedDensity() : 1.0f;
    // Impostors are the CPU's strands' axes: not while the GPU carries the strands (Follow Scalp) or
    // a GPU preview moves them.
    // (The optionVar groomLabLodImpostors 0 turns them off: a look at the same view without them.)
    bool impostorsOptionSet = false;
    const int impostorsOption = MGlobal::optionVarIntValue("groomLabLodImpostors", &impostorsOptionSet);
    settings.impostors = !follow_ && !gpuRunning() && (!impostorsOptionSet || impostorsOption != 0);
    // The flat lines' effect writes coverage masks on a multisampled target (nothing blends there);
    // the lit graph always blends its top.
    // (GroomLab's hair effect writes coverage masks as the flat lines' does.)
    const bool lit = (base.shaderKind == 2 || base.shaderKind == 9) && !base.hair;
    settings.splitBlended = lit || !lodAntiAliased_;
    Lap choose;
    const StrandLod::Selection& sel = lod_.select(view, settings);
    stats.selectMs = choose();
    stats.farLevel = view.farLevel;
    MHWRender::MRenderItem* baseItem = container.find(baseName);
    const auto bound = boundStreams_.find(baseName.asChar());
    const std::uint64_t baseSerial = boundSerials_[baseName.asChar()];
    stats.level = view.level;
    stats.totalStrands = sel.totalStrands;
    stats.groups = lod_.groupCount();
    stats.clumped = lod_.clumped();
    if (!sel.active || !baseItem || bound == boundStreams_.end()) {
        hideLod(container);
        stats.drawnStrands = sel.totalStrands;
        stats.thinnedGroups = stats.impostors = stats.blendSegments = 0;
        stats.ms = lap();
        return;
    }
    // The streams the strands' item was given last (its own, a GPU preview's, the carried points);
    // the classes draw them as they are.
    MHWRender::MVertexBufferArray streams;
    std::vector<const MHWRender::MVertexBuffer*> boundBuffers;
    unsigned vertices = 0;
    for (const auto& [name, buffer] : bound->second) {
        if (!buffer) continue;
        // (A custom handle's vertexCount is not its points' under DirectX 11: a GPU preview's
        // buffers, the GPU lines' streams, made for these points.)
        if (buffer->descriptor().semantic() == MHWRender::MGeometry::kPosition) {
            vertices = buffer->hasCustomResourceHandle() ? static_cast<unsigned>(lines.vertexCount()) : buffer->vertexCount();
        }
        streams.addBuffer(name, buffer);
        boundBuffers.push_back(buffer);
    }
    if (vertices != lines.vertexCount() || vertices == 0) {
        hideLod(container);
        stats.ms = lap();
        return;
    }
    // An item per class (its segments that do not blend, every fragment), and class 0's blendable
    // segments drawn by two: their opaque fragments with the rest, their blended ones after
    // everything (the blend item). The strands' own item off.
    // The classes' indices, each written when its strands changed (or the item they stand in for).
    double indicesMs = 0.0, megabytes = 0.0;
    if (sel.key != lodKey_ || lodBase_ != baseName) {
        Lap write;
        std::size_t written = 0;
        for (std::size_t slot = 0; slot < lodIndices_.size(); ++slot) {
            const bool blended = slot == kLodBlendSlot;
            const int cls = blended ? 0 : static_cast<int>(slot);
            const std::size_t segments = sel.slotSegments[slot];
            if (segments == 0) {
                lodIndices_[slot].reset();
                lodSlotKeys_[slot] = 0;
                continue;
            }
            if (lodIndices_[slot] && lodSlotKeys_[slot] == sel.slotKeys[slot] && lodBase_ == baseName) continue;
            if (!lodIndices_[slot]) lodIndices_[slot] = std::make_unique<MHWRender::MIndexBuffer>(MHWRender::MGeometry::kUnsignedInt32, softwareStaged());
            auto* pairs = static_cast<std::uint32_t*>(lodIndices_[slot]->acquire(static_cast<unsigned>(2 * segments), true));
            if (!pairs) {
                hideLod(container);
                throw std::runtime_error("out of video memory for the level of detail's indices");
            }
            lod_.writeSegments(cls, blended ? kLodBlended : kLodOpaque, pairs);
            lodIndices_[slot]->commit(pairs);
            lodSlotKeys_[slot] = sel.slotKeys[slot];
            megabytes += 8.0 * static_cast<double>(segments) / 1e6;
            ++written;
        }
        lodKey_ = sel.key;
        indicesMs = write();
        if (written > 0) ++stats.rebuilds;
        stats.slotsWritten = written;
    }
    lodBase_ = baseName;
    // Each class's item: the strands' shader copied and styled as theirs, the class's exponent.
    const MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
    const MHWRender::MShaderManager* shaders = renderer ? renderer->getShaderManager() : nullptr;
    const DisplayItem& built = builder_.frame().item(which == 0 ? DisplayItemId::Strands : DisplayItemId::ColoredStrands);
    const MBoundingBox bounds = shape_->boundingBox();
    std::size_t drawnItems = 0;
    for (std::size_t slot = 0; slot < lodShaders_.size(); ++slot) {
        const MString name = lodItemName(slot);
        MHWRender::MRenderItem* item = container.find(name);
        const std::unique_ptr<MHWRender::MIndexBuffer>& indices = lodIndices_[std::min(slot, kLodBlendSlot)];
        if (!indices) {
            if (item) container.remove(name);
            if (lodShaders_[slot] && shaders) shaders->releaseShader(lodShaders_[slot]);
            lodShaders_[slot] = nullptr;
            lodBound_[slot] = LodBinding{};
            continue;
        }
        const bool blended = slot == kLodBlendSlot;
        // The copy kept while the strands' shader is the same (styled again below): a new shader
        // each update would cost its making and, under DirectX 11, move the item in Viewport 2.0's
        // drawing order (the strands' roots and the scalp tie in depth).
        MHWRender::MShaderInstance* shader = lodShaders_[slot];
        if (!shader || lodShaderOf_[slot] != base.shaderSerial) {
            // Made as the strands' own was (updateItem), its settings then hers (style below).
            shader = nullptr;
            if (shaders) {
                switch (base.shaderKind) {
                    case 0:
                    case 1: shader = acquireLinesShader(*shaders, base.shaderKind == 1); break;
                    case 2:
                    case 9: shader = acquireLitShader(*shaders, base.shaderKind, true); break;
                    default: break;
                }
            }
            if (!shader) continue;
            shader->setIsTransparent(false);
            if (!base.hair) hairShaderDefaults(*shader, base.shaderKind);
        }
        if (!item) {
            item = MHWRender::MRenderItem::Create(name, MHWRender::MRenderItem::MaterialSceneItem, MHWRender::MGeometry::kLines);
            item->setDrawMode(MHWRender::MGeometry::kAll);
            container.add(item);
        }
        item->depthPriority(baseItem->depthPriority());
        if (shader != lodShaders_[slot]) {
            item->setShader(shader);  // swap first, then let the old copy go
            if (lodShaders_[slot] && shaders) shaders->releaseShader(lodShaders_[slot]);
            lodShaders_[slot] = shader;
            lodShaderOf_[slot] = base.shaderSerial;
        }
        // Styled as the strands' item (the copy in its shader's place for the call).
        std::swap(base.shader, lodShaders_[slot]);
        style(*item, base, built.color.data(), built.lineWidth);
        std::swap(base.shader, lodShaders_[slot]);
        shader->setParameter("lineLod", slot == 0 || slot >= kLodBlendSlot ? 0.0f : lodClassExponent(static_cast<int>(slot)));
        // The blendable segments: their opaque fragments alone with the rest; their blended ones
        // alone after everything else, depth not written (the transparent pass; coveragePreDraw).
        shader->setParameter("lineBlend", blended ? 2.0f : slot == kLodBlendOpaqueSlot ? 1.0f : 0.0f);
        shader->setIsTransparent(blended);
        if (blended) item->castsShadows(false);
        // Bound again for other streams, indices or a new item, and after each binding of the
        // strands' item (the streams it was given then: the same addresses can name buffers made
        // anew, and a staged buffer written again can be another on the device); not otherwise:
        // binding a staged buffer again has Maya send its system-memory copy up, over what a GPU
        // preview wrote into it (DirectX 11; applyGpuPreview binds the strands' item for the same
        // reason only when needed).
        LodBinding& was = lodBound_[slot];
        const std::uint64_t indicesKey = hashCombine(lodSlotKeys_[std::min(slot, kLodBlendSlot)], reinterpret_cast<std::uintptr_t>(indices.get()));
        if (!was.valid || was.streams != boundBuffers || was.baseSerial != baseSerial || was.indicesKey != indicesKey ||
            !was.bounds.contains(bounds.min()) || !was.bounds.contains(bounds.max()) || item != was.item) {
            setGeometryForRenderItem(*item, streams, *indices, &bounds);
            was.valid = true;
            was.streams = boundBuffers;
            was.baseSerial = baseSerial;
            was.indicesKey = indicesKey;
            was.bounds = bounds;
            was.item = item;
            ++stats.slotsBound;
        }
        item->enable(true);
        ++drawnItems;
    }
    baseItem->enable(false);
    lodOn_ = true;
    // The impostors: their own small item, drawn as the strands are, hashed alone (lineLod 1) at
    // their own widths (the band's ink at the render's density: lineDensity 1).
    const StrandsPtr axes = lod_.impostors();
    std::shared_ptr<const StrandColors> axisColors =
        axes && lines.colors ? lod_.impostorColors(*lines.colors) : nullptr;
    const std::uint64_t colorsKey = axisColors ? axisColors->key : 0;
    if (!axes) {
        dropLodImpostors(container);
    } else if (sel.impostorKey != lodImpostorKey_ || colorsKey != lodImpostorColors_ || !container.find(kLodImpostorsItem)) {
        auto geometry = std::make_shared<DisplayGeometry>();
        geometry->lines = std::make_shared<StrandLines>(axes, lines.scalp, lines.lit, axisColors);
        updateItem(container, kLodImpostorsItem, built.color.data(), baseItem->depthPriority(), geometry, built.lineWidth);
        lodImpostorKey_ = sel.impostorKey;
        lodImpostorColors_ = colorsKey;
    } else {
        restyleItem(container, kLodImpostorsItem, built.color.data(), built.lineWidth);
    }
    if (axes) {
        ItemBuffers& impostors = items_[kLodImpostorsItem.asChar()];
        if (impostors.shader && impostors.coverage) {
            impostors.shader->setParameter("lineDensity", 1.0f);
            impostors.shader->setParameter("lineLod", 1.0f);
        }
    }
    stats.on = true;
    stats.drawnStrands = sel.drawnStrands;
    stats.thinnedGroups = sel.thinnedGroups;
    stats.impostors = axes ? axes->strandCount() : 0;
    stats.blendSegments = sel.blendSegments;
    stats.items = drawnItems;
    stats.baseSerial = baseSerial;
    stats.staleSlots = 0;  // (drawn slots bound before the strands' item's last binding: none, by the above)
    for (std::size_t slot = 0; slot < lodBound_.size(); ++slot) {
        if (lodShaders_[slot] && container.find(lodItemName(slot)) && lodBound_[slot].baseSerial != baseSerial) ++stats.staleSlots;
    }
    stats.indicesMs = indicesMs;
    stats.megabytes = megabytes;
    ++stats.updates;
    stats.ms = lap();
}
}  // namespace groomlab::maya
