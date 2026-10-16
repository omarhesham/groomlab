// SPDX-License-Identifier: MIT
// GroomLab SDK sample (MIT licence, LICENSE.txt): free to copy into your own bridge.
//
// groom_to_obj: a minimal host bridge. It loads a .groom file (or starts from the default graph),
// optionally replaces the scalp with an OBJ mesh, evaluates at a frame and writes the strands as
// OBJ polylines: one "v" per point, one "l" per strand. It uses only the C++ wrapper.
//
//   groom_to_obj [--groom hair.groom] [--scalp head.obj] [--frame 1] [--density 1] out.obj
//
// Points come out in the groom's units and up axis (cm and Y up unless the file says otherwise).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "groomlab/groomlab.hpp"

namespace gl = groomlab::api;

namespace {

struct ObjMesh {
    std::vector<float> points;                // x y z per "v"
    std::vector<float> uvs;                   // u v per "vt"
    std::vector<std::int32_t> faceCounts;     // corners per "f"
    std::vector<std::int32_t> faceIndices;    // point index per corner
    std::vector<std::int32_t> faceUvIndices;  // uv index per corner, -1 for none
};

// An OBJ index (1-based, or negative: counted back from the last one so far) made 0-based.
std::int32_t objIndex(const std::string& token, std::size_t count) {
    const long i = std::strtol(token.c_str(), nullptr, 10);
    return static_cast<std::int32_t>(i < 0 ? static_cast<long>(count) + i : i - 1);
}

ObjMesh readObj(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot read " + path);
    ObjMesh mesh;
    for (std::string line; std::getline(in, line);) {
        std::istringstream s(line);
        std::string tag;
        s >> tag;
        if (tag == "v") {
            float x = 0, y = 0, z = 0;
            s >> x >> y >> z;
            mesh.points.insert(mesh.points.end(), {x, y, z});
        } else if (tag == "vt") {
            float u = 0, v = 0;
            s >> u >> v;
            mesh.uvs.insert(mesh.uvs.end(), {u, v});
        } else if (tag == "f") {
            std::int32_t corners = 0;
            for (std::string corner; s >> corner; ++corners) {  // v, v/vt, v/vt/vn or v//vn
                const std::size_t slash = corner.find('/');
                mesh.faceIndices.push_back(objIndex(corner.substr(0, slash), mesh.points.size() / 3));
                std::string vt;
                if (slash != std::string::npos) vt = corner.substr(slash + 1, corner.find('/', slash + 1) - slash - 1);
                mesh.faceUvIndices.push_back(vt.empty() ? -1 : objIndex(vt, mesh.uvs.size() / 2));
            }
            mesh.faceCounts.push_back(corners);
        }
    }
    return mesh;
}

// The OBJ as the session's scalp. UVs go in face-varying (u v per corner) when every corner has one.
void setScalp(gl::Session& session, const ObjMesh& mesh) {
    std::vector<float> uvs;
    bool everyCorner = !mesh.uvs.empty();
    for (const std::int32_t i : mesh.faceUvIndices) {
        everyCorner = everyCorner && i >= 0 && 2 * static_cast<std::size_t>(i) + 1 < mesh.uvs.size();
    }
    if (everyCorner) {
        for (const std::int32_t i : mesh.faceUvIndices) uvs.insert(uvs.end(), {mesh.uvs[2 * i], mesh.uvs[2 * i + 1]});
    }
    session.setScalp(mesh.points, mesh.faceCounts, mesh.faceIndices, uvs);
}

void writeObj(const std::string& path, const gl::Strands& strands) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot write " + path);
    out << "# GroomLab strands: " << strands.strandCount() << " strands, " << strands.pointCount() << " points\n";
    const gl::View<float> p = strands.positions();
    char line[128];
    for (std::size_t i = 0; i < strands.pointCount(); ++i) {
        std::snprintf(line, sizeof(line), "v %.9g %.9g %.9g\n", p[3 * i], p[3 * i + 1], p[3 * i + 2]);
        out << line;
    }
    const gl::View<std::uint64_t> offsets = strands.offsets();  // strand s: points offsets[s] to offsets[s + 1] - 1
    for (std::size_t s = 0; s < strands.strandCount(); ++s) {
        out << 'l';
        for (std::uint64_t k = offsets[s]; k < offsets[s + 1]; ++k) out << ' ' << k + 1;  // OBJ counts from 1
        out << '\n';
    }
    if (!out) throw std::runtime_error("cannot write " + path);
}

int usage() {
    std::cerr << "usage: groom_to_obj [--groom file.groom] [--scalp mesh.obj] [--frame F] [--density D] out.obj\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::string groomPath, scalpPath, outPath;
    double frame = 1.0;
    float density = 1.0f;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const bool value = i + 1 < argc;
        if (arg == "--groom" && value) groomPath = argv[++i];
        else if (arg == "--scalp" && value) scalpPath = argv[++i];
        else if (arg == "--frame" && value) frame = std::atof(argv[++i]);
        else if (arg == "--density" && value) density = static_cast<float>(std::atof(argv[++i]));
        else if (outPath.empty() && !arg.empty() && arg[0] != '-') outPath = arg;
        else return usage();
    }
    if (outPath.empty() || (groomPath.empty() && scalpPath.empty())) return usage();

    try {
        gl::Session session;  // throws when the library's API major differs from this header's
        if (!groomPath.empty()) session.loadFile(groomPath);  // graph, guides, scalp, maps, frame rate
        if (!scalpPath.empty()) setScalp(session, readObj(scalpPath));
        session.setFrame(frame);  // seconds = frame / the groom's frames per second

        gl::EvalOptions options;
        options.density = density;
        const gl::Strands strands = session.evaluate(options);
        const std::string problems = session.evalErrorsJson();  // a partial result still evaluates
        if (problems != "[]") std::cerr << "groom_to_obj: warnings: " << problems << "\n";

        writeObj(outPath, strands);
        std::cout << "groom_to_obj: " << strands.strandCount() << " strands, " << strands.pointCount() << " points -> "
                  << outPath << "\n";
        return 0;
    } catch (const gl::Error& e) {
        std::cerr << "groom_to_obj: " << e.what() << "\n";  // e.code(): the gl_result
    } catch (const std::exception& e) {
        std::cerr << "groom_to_obj: " << e.what() << "\n";
    }
    return 1;
}
