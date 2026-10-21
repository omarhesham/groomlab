// SPDX-License-Identifier: Apache-2.0
#pragma once

// Frames rendered without a window: a QRhi of its own (Direct3D 11 on Windows, else the Null
// backend), a texture target, the renderer's frames into it, the last one read back. The
// tests, the --screenshot and --benchmark runs go through here, so none of them needs the
// window to be exposed (the offscreen platform never draws a QRhiWidget).

#include <memory>

#include <QImage>
#include <QSize>
#include <QString>
#include <rhi/qrhi.h>

#include "camera.h"
#include "renderer.h"

namespace groomlab::app {

struct OffscreenResult {
    QImage image;          // null on the Null backend (nothing is read back there)
    double msPerFrame = 0.0;  // of the frames after the first (uploads aside), the GPU waited for
    double firstFrameMs = 0.0;
    // The GPU's own time per frame (timestamps around the command buffer, the frames after the
    // first; 0 where the backend has none): what the frame costs the GPU, without the CPU's side.
    double gpuMsPerFrame = 0.0;
    QString api;
    int frames = 0;
};

// The device: GROOMLAB_APP_RHI picks it as the viewport's (d3d11, d3d12, vulkan, opengl, metal,
// null), else Direct3D 11 on Windows, Metal on macOS; Null when none can be made.
std::unique_ptr<QRhi> createOffscreenRhi();

// A target to render frames into without a window, kept between frames (a benchmark's frame
// after every dab or drag step, a test comparing frames): the QRhi (its own when none is
// given) and an RGBA8 texture with a depth buffer at `size`; `samples` > 1: drawn into
// multisampled buffers (as the viewport's) resolved into the texture.
class OffscreenView {
public:
    explicit OffscreenView(const QSize& size, QRhi* rhi = nullptr, int samples = 1);
    ~OffscreenView();
    OffscreenView(const OffscreenView&) = delete;
    OffscreenView& operator=(const OffscreenView&) = delete;

    QRhi* rhi() const { return rhi_; }
    bool ready() const { return target_ != nullptr; }
    const QSize& size() const { return size_; }
    QRhiTextureRenderTarget* target() const { return target_.get(); }
    QRhiTexture* texture() const { return texture_.get(); }
    // One frame of `renderer` with `camera`, the GPU waited for (the time is the whole frame's);
    // `readback`: the pixels come back in the result (else a null image: faster).
    OffscreenResult frame(GroomRenderer& renderer, const OrbitCamera& camera, bool readback = true);

private:
    std::unique_ptr<QRhi> own_;
    QRhi* rhi_ = nullptr;
    QSize size_;
    std::unique_ptr<QRhiTexture> texture_;
    std::unique_ptr<QRhiRenderBuffer> depth_;
    std::unique_ptr<QRhiRenderBuffer> color_;  // multisampled
    std::unique_ptr<QRhiTextureRenderTarget> target_;
    std::unique_ptr<QRhiRenderPassDescriptor> pass_;
};

// `frames` frames of the renderer at `size` (RGBA8, a depth buffer, `samples` samples a pixel) on
// `rhi` (made here when null, and the renderer's GPU objects released before it goes).
OffscreenResult renderOffscreen(GroomRenderer& renderer, const OrbitCamera& camera, const QSize& size, int frames,
                                QRhi* rhi = nullptr, int samples = 1);

}  // namespace groomlab::app
