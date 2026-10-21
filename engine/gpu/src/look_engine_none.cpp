// SPDX-License-Identifier: Apache-2.0
// The GPU look preview where it has no backend yet (macOS, Linux): the engine's planning, its run
// algorithm and LineFrames' shared part build as everywhere (look_chain.cpp, look_preview.cpp,
// look_engine.cpp, line_frames.cpp), but no device runs them. Every factory below throws, as a real
// one does on a device it cannot use, and OpenGL's kernels are never ready: the hosts catch that and
// preview on the CPU (Maya's gpu_preview.cpp and gpu_lines.cpp, the app's gpu_preview.cpp).
//
// A Metal backend plugs in beside this file as look_engine_metal.mm (and line_frames' Metal class),
// mirroring look_engine_d3d11.cpp: the engine's few primitives (buffers by number, a dispatch, a
// copy, timestamps) on an MTLDevice and its command queue, the kernels as Metal Shading Language
// (slangc -target metal, as the gpu.kernel_targets test compiles them already), and a
// makeMetalLookEngine(void* device) here in look_engine.h's list (docs/MACOS.md, "GPU").

#include <stdexcept>
#include <string>

#include "groomlab/gpu/line_frames.h"
#include "groomlab/gpu/look_engine.h"

namespace groomlab::gpu {

namespace {

[[noreturn]] void noBackend(const char* what) {
    throw std::runtime_error(std::string("GPU look preview: no ") + what + " backend on this platform (the CPU previews)");
}

}  // namespace

std::unique_ptr<LookEngine> makeD3D11LookEngine(ID3D11Device*) { noBackend("DirectX 11"); }
ID3D11Buffer* createD3D11DisplayBuffer(ID3D11Device*, std::size_t) { noBackend("DirectX 11"); }
std::unique_ptr<LookEngine> makeGLLookEngine() { noBackend("OpenGL"); }

bool prepareGLLookKernels() { return false; }

bool glLookKernelsReady(const LookRun&, std::string* why) {
    if (why) *why = "no OpenGL backend on this platform";
    return false;
}

bool glLookKernelsAllReady() { return false; }
void forgetGLLookKernels() {}

std::unique_ptr<LineFrames> makeD3D11LineFrames(ID3D11Device*) { noBackend("DirectX 11"); }
std::unique_ptr<LineFrames> makeGLLineFrames() { noBackend("OpenGL"); }
ID3D11Buffer* createD3D11IndexBuffer(ID3D11Device*, std::size_t) { noBackend("DirectX 11"); }

}  // namespace groomlab::gpu
