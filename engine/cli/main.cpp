// SPDX-License-Identifier: Apache-2.0
// Headless entry point (OR-4): evaluates groom files without Maya and writes
// the strands to Alembic or USD, frame by frame. Also inspects groom and
// Alembic files. Never online.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "groomlab/frame_time.h"
#include "groomlab/graph.h"
#include "groomlab/groom_file.h"
#include "groomlab/guide_hierarchy.h"
#include "groomlab/random.h"
#include "groomlab/scale_maps.h"
#include "groomlab/scales.h"
#include "groomlab/usd_writer.h"
#include "groomlab/version.h"

#if GROOMLAB_HAS_ALEMBIC
#include "groomlab/alembic_io.h"
#include "groomlab/alembic_operators.h"
#include "groomlab/groom_scene.h"
#endif

namespace {

void printUsage() {
    std::cout << "usage: groomlab-cli --version\n"
                 "       groomlab-cli --list-operators\n"
#if GROOMLAB_HAS_ALEMBIC
                 "       groomlab-cli info <file.groom>\n"
                 "       groomlab-cli eval <file.groom> -o <out.abc|out.usda> [-f <frame>|<start>-<end>]\n"
                 "                    [--step <frames>] [--density <0-1>] [--timings] [--stats]\n"
                 "                    [--scales mesh|instancer|both|none]\n"
                 "       groomlab-cli bake-scales <file.groom> -o <prefix> [-f <frame>] [--resolution <16-8192>]\n"
                 "                    [--max-height <h>] [--padding <texels>] [--background r,g,b]\n"
                 "       groomlab-cli abc-info <file.abc> [object]\n"
                 "       groomlab-cli hash <file.abc> [object]\n"
#endif
        ;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

#if GROOMLAB_HAS_ALEMBIC

int abcInfo(const char* path, const char* name) {
    try {
        const auto frames = groomlab::readAlembicCurves(path, name ? name : "");
        std::cout << frames.size() << " frame(s)\n";
        for (const auto& f : frames) {
            groomlab::Vec3f lo(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                               std::numeric_limits<float>::max());
            groomlab::Vec3f hi = -lo;
            for (const auto& p : f.strands.positions()) {
                lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
                hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
            }
            std::cout << "  t=" << f.seconds << "s strands=" << f.strands.strandCount()
                      << " points=" << f.strands.pointCount();
            if (!f.strands.empty()) {
                std::cout << " bbox=(" << lo.x << "," << lo.y << "," << lo.z << ")-(" << hi.x << "," << hi.y << ","
                          << hi.z << ")";
            }
            std::cout << "\n";
        }
        return 0;
    } catch (const groomlab::AlembicError& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

// An exact hash per frame of the strand IDs, point counts, positions,
// widths and UVs (their float bits), and one of the whole file: two exports of the
// same groom, from Maya and from here, match when their hashes match, with no
// rounding in between. Alembic stores floats exactly, so this is bit-exact.
int abcHash(const char* path, const char* name) {
    try {
        const auto frames = groomlab::readAlembicCurves(path, name ? name : "");
        std::uint64_t all = groomlab::hash64(frames.size());
        for (const auto& f : frames) {
            const groomlab::StrandSet& s = f.strands;
            std::uint64_t h = groomlab::hash64(s.strandCount());
            for (const groomlab::StrandId id : s.ids()) h = groomlab::hashCombine(h, id);
            for (const std::size_t offset : s.offsets()) h = groomlab::hashCombine(h, offset);
            for (const groomlab::Vec3f& p : s.positions()) {
                h = groomlab::hashFloat(h, p.x);
                h = groomlab::hashFloat(h, p.y);
                h = groomlab::hashFloat(h, p.z);
            }
            for (const float w : s.widths()) h = groomlab::hashFloat(h, w);
            for (const auto& uv : f.uvs) h = groomlab::hashFloat(groomlab::hashFloat(h, uv[0]), uv[1]);
            all = groomlab::hashCombine(all, h);
            std::cout << "t=" << f.seconds << "s strands=" << s.strandCount() << " points=" << s.pointCount()
                      << " uvs=" << f.uvs.size() << " hash=" << std::hex << std::setw(16) << std::setfill('0') << h << std::dec << "\n";
        }
        std::cout << "all=" << std::hex << std::setw(16) << std::setfill('0') << all << std::dec << "\n";
        return 0;
    } catch (const groomlab::AlembicError& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

int groomInfo(const char* path) {
    try {
        const groomlab::GroomFile file = groomlab::readGroomFile(path);
        std::cout << "source:      " << (file.source.empty() ? "-" : file.source) << "\n"
                  << "fps:         " << file.framesPerSecond << "\n"
                  << "units:       " << file.units << ", " << file.upAxis << " up\n";
        if (const auto& scalp = file.data.scalp) {
            std::cout << "scalp:       " << scalp->pointCount() << " points, " << scalp->triangleCount() << " triangles"
                      << (scalp->hasRest() ? ", rest pose" : "") << (scalp->hasUVs() ? ", UVs" : "");
            for (const std::string& c : scalp->colorSetNames()) std::cout << ", color set " << c;
            std::cout << "\n";
        } else {
            std::cout << "scalp:       none\n";
        }
        if (!file.data.hierarchyJson.empty()) {
            const auto h = groomlab::GuideHierarchy::fromJson(file.data.hierarchyJson);
            std::cout << "hierarchy:   " << h.guides().size() << " guides in " << h.groups().size() << " groups\n";
        }
        std::cout << "flat guides: " << (file.data.guides ? file.data.guides->size() : 0) << "\n";
        if (file.data.followScalp) std::cout << "follow scalp: on (evaluated at rest, carried by each root)\n";
        const groomlab::GraphDocument doc = groomlab::loadGraphJson(file.data.graphJson);
        std::cout << "graph:       " << doc.nodesByName.size() << " nodes, output " << doc.nameOf(doc.output) << "\n";
        if (!file.scalpCache.empty()) {
            std::cout << "scalp cache: " << file.scalpCache.file
                      << (file.scalpCache.object.empty() ? "" : " (" + file.scalpCache.object + ")") << "\n";
        }
        if (!file.guideCache.empty()) {
            std::cout << "guide cache: " << file.guideCache.file
                      << (file.guideCache.object.empty() ? "" : " (" + file.guideCache.object + ")") << "\n";
        }
        if (!file.cardBake.empty()) std::cout << "card bake:   " << file.cardBake.prefix << "*\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

struct EvalArgs {
    std::string groom;
    std::string out;
    double start = 1.0;
    double end = 1.0;
    double step = 1.0;
    float density = 1.0f;
    bool timings = false;  // print the milliseconds each graph node took, per frame
    bool stats = false;    // print the session's stats (GroomStats JSON) after the first frame
    std::string scales = "mesh";  // how the scales are written (USD: mesh, instancer, both, none)
};

// The groom file's own prototype meshes (GroomFile::prototypes) by name.
groomlab::PrototypeLookup storedPrototypes(const groomlab::GroomScene& scene) {
    return [&scene](const std::string& name) -> const groomlab::InstancePrototype* {
        const auto it = scene.file().prototypes.find(name);
        return it != scene.file().prototypes.end() && it->second.found ? &it->second : nullptr;
    };
}

bool parseFrames(const std::string& s, double* start, double* end) {
    // "12", "1-24", "-5--1" (negative frames): split at a '-' that follows a digit.
    char* rest = nullptr;
    *start = std::strtod(s.c_str(), &rest);
    if (rest == s.c_str()) return false;
    if (*rest == '\0') {
        *end = *start;
        return true;
    }
    if (*rest != '-') return false;
    const char* second = rest + 1;
    *end = std::strtod(second, &rest);
    return rest != second && *rest == '\0';
}

int evalGroom(int argc, char** argv) {
    EvalArgs a;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument(arg + " needs a value");
            return argv[++i];
        };
        if (arg == "-o" || arg == "--out") {
            a.out = value();
        } else if (arg == "-f" || arg == "--frames") {
            if (!parseFrames(value(), &a.start, &a.end)) throw std::invalid_argument("bad frame range");
        } else if (arg == "--step") {
            a.step = std::stod(value());
        } else if (arg == "--density") {
            a.density = std::stof(value());
        } else if (arg == "--timings") {
            a.timings = true;
        } else if (arg == "--stats") {
            a.stats = true;
        } else if (arg == "--scales") {
            a.scales = value();
            if (a.scales != "mesh" && a.scales != "instancer" && a.scales != "both" && a.scales != "none") {
                throw std::invalid_argument("--scales must be mesh, instancer, both or none");
            }
        } else if (!arg.empty() && arg[0] == '-') {
            throw std::invalid_argument("unknown option " + arg);
        } else if (a.groom.empty()) {
            a.groom = arg;
        } else {
            throw std::invalid_argument("unexpected argument " + arg);
        }
    }
    if (a.groom.empty() || a.out.empty()) throw std::invalid_argument("eval needs a groom file and -o <output>");
    if (a.step <= 0.0) throw std::invalid_argument("--step must be positive");
    if (a.end < a.start) throw std::invalid_argument("the end frame is before the start frame");
    if (a.density < 0.0f || a.density > 1.0f) throw std::invalid_argument("--density must be between 0 and 1");
    const bool usd = endsWith(a.out, ".usda") || endsWith(a.out, ".usd");
    if (!usd && !endsWith(a.out, ".abc")) throw std::invalid_argument("the output must be .abc or .usda");

    const auto t0 = std::chrono::steady_clock::now();
    groomlab::GroomScene scene(a.groom);
    const double fps = scene.framesPerSecond();
    const auto frameCount = static_cast<std::size_t>(std::floor((a.end - a.start) / a.step + 1e-6)) + 1;

    std::unique_ptr<groomlab::AlembicCurvesWriter> abc;
    std::unique_ptr<groomlab::UsdCurvesWriter> usdWriter;
    const std::string name = "groom";
    if (!usd && (a.scales == "instancer" || a.scales == "both")) throw std::invalid_argument("an instancer goes to USD");
    if (usd) {
        groomlab::UsdCurvesWriter::Options options;
        options.framesPerSecond = fps;
        options.comment = scene.file().source;
        // The file's own up axis and unit (a Z-up groom stays Z-up; meters stay meters).
        options.upAxis = scene.file().upAxis == "Z" ? "Z" : "Y";
        const std::string& unit = scene.file().units;
        options.metersPerUnit = unit == "m" ? 1.0 : unit == "mm" ? 0.001 : unit == "in" ? 0.0254 : unit == "ft" ? 0.3048 : 0.01;
        using S = groomlab::UsdCurvesWriter::Options::Scales;
        options.scales = a.scales == "instancer" ? S::Instancer : a.scales == "both" ? S::Both : a.scales == "none" ? S::None : S::Mesh;
        options.prototypes = storedPrototypes(scene);
        usdWriter = std::make_unique<groomlab::UsdCurvesWriter>(a.out, name, options);
    } else {
        abc = std::make_unique<groomlab::AlembicCurvesWriter>(a.out, name, groomlab::frameToSeconds(a.start, fps),
                                                             groomlab::frameToSeconds(a.step, fps));
    }

    std::size_t strandTotal = 0;
    // A groom that nothing moves (GroomScene::readsTime) is evaluated once, for every frame.
    const bool moves = scene.readsTime();
    groomlab::StrandsPtr still;
    groomlab::ScaleMeshBuilder scales;  // bends only the scales that moved since the last frame
    for (std::size_t i = 0; i < frameCount; ++i) {
        const double frame = a.start + static_cast<double>(i) * a.step;
        const double seconds = groomlab::frameToSeconds(frame, fps);  // every host's rule (frame_time.h)
        const groomlab::StrandsPtr strands = moves || i == 0 ? scene.evaluate(seconds, a.density) : still;
        if (i == 0) still = strands;
        const groomlab::StrandSet empty;
        const groomlab::StrandSet& s = strands ? *strands : empty;
        const groomlab::MeshPtr scalp = scene.rootScalpAt(seconds);
        const bool scaled = a.scales != "none" && s.instances() && s.instances()->hasMeshes();
        const auto scaleMesh = scaled ? scales.build(s, scalp, storedPrototypes(scene)) : nullptr;
        if (usdWriter) {
            usdWriter->writeFrame(frame, s, scalp.get(), scaleMesh.get());
        } else {
            abc->writeFrame(s, scalp.get(), scaleMesh.get());  // root UVs, as Maya's export writes them
        }
        strandTotal += s.strandCount();
        std::cout << "frame " << frame << ": " << s.strandCount() << " strands\n";
        if (i == 0) {  // what the nodes have to say (a Jiggle without scalp motion), once
            for (const auto& [node, note] : scene.nodeNotes()) std::cout << "  note: " << node << ": " << note << "\n";
            if (a.stats) std::cout << "stats: " << scene.session().stats().toJson() << "\n";
        }
        if (a.timings) {
            auto times = scene.nodeTimes();
            std::sort(times.begin(), times.end(), [](const auto& x, const auto& y) { return x.second > y.second; });
            for (const auto& [node, ms] : times) std::cout << "  " << node << ": " << ms << " ms\n";
        }
    }
    if (usdWriter) usdWriter->close();
    if (abc) abc->close();
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::cout << "wrote " << frameCount << " frame(s), " << strandTotal / frameCount << " strands per frame on average, to "
              << a.out << " in " << static_cast<long long>(ms) << " ms\n";
    return 0;
}

// The scales of a groom file at a frame baked onto its scalp's uvs (scale_maps.h).
int bakeScales(int argc, char** argv) {
    std::string groom, prefix;
    double frame = 1.0;
    groomlab::ScaleMapOptions options;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        auto value = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument(arg + " needs a value");
            return argv[++i];
        };
        if (arg == "-o" || arg == "--out") {
            prefix = value();
        } else if (arg == "-f" || arg == "--frame") {
            frame = std::stod(value());
        } else if (arg == "--resolution") {
            options.resolution = std::stoi(value());
        } else if (arg == "--max-height") {
            options.maxHeight = std::stof(value());
        } else if (arg == "--padding") {
            options.padding = std::stoi(value());
        } else if (arg == "--background") {  // r,g,b: the color map between scales (linear)
            std::string rgb = value();
            std::replace(rgb.begin(), rgb.end(), ',', ' ');
            std::istringstream in(rgb);
            if (!(in >> options.background[0] >> options.background[1] >> options.background[2])) {
                throw std::invalid_argument("--background needs r,g,b");
            }
        } else if (!arg.empty() && arg[0] == '-') {
            throw std::invalid_argument("unknown option " + arg);
        } else if (groom.empty()) {
            groom = arg;
        } else {
            throw std::invalid_argument("unexpected argument " + arg);
        }
    }
    if (groom.empty() || prefix.empty()) throw std::invalid_argument("bake-scales needs a groom file and -o <prefix>");
    const auto t0 = std::chrono::steady_clock::now();
    groomlab::GroomScene scene(groom);
    const double seconds = groomlab::frameToSeconds(frame, scene.framesPerSecond());
    const groomlab::StrandsPtr strands = scene.evaluate(seconds, 1.0f);
    const groomlab::MeshPtr scalp = scene.rootScalpAt(seconds);
    const auto mesh = strands ? groomlab::buildScaleMesh(*strands, scalp, storedPrototypes(scene)) : nullptr;
    if (!mesh || !scalp) throw std::runtime_error("the groom has no scales at frame " + std::to_string(frame));
    const groomlab::ScaleMaps maps = groomlab::bakeScaleMaps(*scalp, *mesh, options);
    for (const std::string& file : groomlab::writeScaleMaps(maps, prefix)) std::cout << "wrote " << file << "\n";
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::cout << mesh->scales.size() << " scales, max height " << maps.maxHeight << ", "
              << (maps.texels ? 100 * maps.covered / maps.texels : 0) << "% of the skin covered, in "
              << static_cast<long long>(ms) << " ms\n";
    return 0;
}

#endif

}  // namespace

int main(int argc, char** argv) {
#if GROOMLAB_HAS_ALEMBIC
    groomlab::registerAlembicOperators(groomlab::OperatorRegistry::instance());
#endif
    if (argc < 2) {
        printUsage();
        return 1;
    }
    const std::string command = argv[1];
    if (command == "--version") {
        std::cout << "groomlab-cli " << groomlab::kVersionString << " (TBB " << (GROOMLAB_HAS_TBB ? "on" : "off")
                  << ", Alembic " << (GROOMLAB_HAS_ALEMBIC ? "on" : "off") << ")\n";
        return 0;
    }
    if (command == "--list-operators") {
        for (const auto& name : groomlab::OperatorRegistry::instance().typeNames()) std::cout << name << "\n";
        return 0;
    }
#if GROOMLAB_HAS_ALEMBIC
    if (command == "abc-info" && argc >= 3) return abcInfo(argv[2], argc >= 4 ? argv[3] : nullptr);
    if (command == "hash" && argc >= 3) return abcHash(argv[2], argc >= 4 ? argv[3] : nullptr);
    if (command == "info" && argc >= 3) return groomInfo(argv[2]);
    if (command == "eval" || command == "bake-scales") {
        try {
            return command == "eval" ? evalGroom(argc, argv) : bakeScales(argc, argv);
        } catch (const std::invalid_argument& e) {
            std::cerr << "error: " << e.what() << "\n";
            printUsage();
            return 2;
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
            return 1;
        }
    }
#endif
    printUsage();
    return 1;
}
