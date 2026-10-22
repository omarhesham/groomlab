// SPDX-License-Identifier: Apache-2.0
// web_probe: WebGroom (web_groom.h) built natively, for checking the WebAssembly build against
// the desktop engine: evaluates a .groom (or a preset with an .obj) at a density and prints what
// engine/web/tools/compare.mjs prints from the browser module: the strand and point counts, a
// hash of the positions' float bits, and a few points.
//
//   groomlab_web_probe <file.groom | preset.json mesh.obj> [--density 0.25] [--set id=value]... [--controls] [--dump positions.f32]

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "web_groom.h"

namespace {

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

// FNV-1a over the floats' bits (compare.mjs hashes the same bytes).
std::uint64_t hashFloats(const std::vector<float>& v) {
    std::uint64_t h = 0xcbf29ce484222325ull;
    const auto* bytes = reinterpret_cast<const unsigned char*>(v.data());
    for (std::size_t i = 0; i < v.size() * sizeof(float); ++i) {
        h ^= bytes[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> files;
    std::vector<std::pair<std::string, double>> sets;
    double density = 0.25;
    bool controls = false;
    std::string dump;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--density" && i + 1 < argc) {
            density = std::stod(argv[++i]);
        } else if (a == "--dump" && i + 1 < argc) {
            dump = argv[++i];
        } else if (a == "--controls") {
            controls = true;
        } else if (a == "--set" && i + 1 < argc) {
            const std::string s = argv[++i];
            const auto eq = s.find('=');
            sets.emplace_back(s.substr(0, eq), std::stod(s.substr(eq + 1)));
        } else {
            files.push_back(a);
        }
    }
    if (files.empty()) {
        std::cerr << "usage: groomlab_web_probe <file.groom | preset.json mesh.obj> [--density d] [--set id=value]\n";
        return 2;
    }
    try {
        groomlab::web::WebGroom groom;
        if (!groom.load(readFile(files[0]), files.size() > 1 ? readFile(files[1]) : std::string())) {
            std::cerr << "error: " << groom.error() << "\n";
            return 1;
        }
        if (controls) {
            std::cout << groom.controlsJson() << "\n";
            if (!groom.error().empty()) std::cerr << "controls: " << groom.error() << "\n";
        }
        for (const auto& [id, value] : sets) {
            if (!groom.setParam(id, value)) std::cerr << "warning: cannot set " << id << "\n";
        }
        if (!groom.evaluate(density)) {
            std::cerr << "error: " << groom.error() << "\n";
            return 1;
        }
        const auto& off = groom.offsets();
        const auto& pos = groom.positions();
        std::cout << std::setprecision(9);
        std::cout << "strands " << groom.strandCount() << "\npoints " << pos.size() / 3 << "\nhash " << std::hex
                  << hashFloats(pos) << std::dec << "\n";
        const std::uint32_t n = groom.strandCount();
        if (!dump.empty()) {
            std::ofstream out(dump, std::ios::binary);
            out.write(reinterpret_cast<const char*>(pos.data()), static_cast<std::streamsize>(pos.size() * sizeof(float)));
        }
        for (const std::uint32_t s : {0u, n / 2, n ? n - 1 : 0u}) {
            if (s >= n) continue;
            const std::uint32_t tip = off[s + 1] - 1;
            std::cout << "strand " << s << " root " << pos[off[s] * 3] << " " << pos[off[s] * 3 + 1] << " " << pos[off[s] * 3 + 2]
                      << " tip " << pos[tip * 3] << " " << pos[tip * 3 + 1] << " " << pos[tip * 3 + 2] << "\n";
        }
        std::cerr << "evaluate " << groom.evaluateMs() << " ms\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
