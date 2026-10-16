// SPDX-License-Identifier: Apache-2.0
#include "groomlab/sim_cache.h"

#include <memory>
#include <stdexcept>

#include "groomlab/alembic_io.h"
#include "groomlab/frame_time.h"

namespace groomlab {

std::size_t writeSimulationCache(const GroomSession& session, const std::string& node, const std::string& path, int first,
                                 int last, const std::function<void(int done, int count)>& progress) {
    if (last < first) throw std::runtime_error("bake " + node + ": no frames (" + std::to_string(first) + " to " + std::to_string(last) + ")");
    const double fps = session.framesPerSecond();
    // Every frame first, so a missing one writes no file.
    for (int f = first; f <= last; ++f) {
        if (!session.simulatedFrame(node, f)) {
            throw std::runtime_error("bake " + node + ": frame " + std::to_string(f) + " is not simulated (Simulate Range to " +
                                     std::to_string(last) + " first)");
        }
    }
    AlembicCurvesWriter::Options options;
    options.pointChannels = true;  // the wetness, with water or rain
    AlembicCurvesWriter writer(path, node, frameToSeconds(first, fps), frameToSeconds(1.0, fps), options);
    const int count = last - first + 1;
    for (int f = first; f <= last; ++f) {
        const GuidesPtr frame = session.simulatedFrame(node, f);
        StrandSet curves;
        bool wet = false;
        for (const Guide& g : frame->guides()) {
            curves.addStrand(g.id, g.points, {}, g.root);
            wet = wet || g.pointAttributes.count(kWetnessChannel) != 0;
        }
        if (wet) {
            std::vector<float>& wetness = curves.addPointChannel(kWetnessChannel, 0.0f);
            for (std::size_t s = 0; s < frame->size(); ++s) {
                const Guide& g = frame->guides()[s];
                const auto it = g.pointAttributes.find(kWetnessChannel);
                if (it == g.pointAttributes.end() || it->second.size() != g.points.size()) continue;
                std::copy(it->second.begin(), it->second.end(), wetness.begin() + static_cast<std::ptrdiff_t>(curves.strandBegin(s)));
            }
        }
        writer.writeFrame(curves);
        if (progress) progress(f - first + 1, count);
    }
    writer.close();
    return static_cast<std::size_t>(count);
}

BakedSimulationPtr readSimulationCache(const std::string& path, const std::string& object, std::uint64_t stamp) {
    auto reader = std::make_shared<const AlembicCurvesReader>(path, object);
    auto baked = std::make_shared<BakedSimulation>();
    baked->file = path;
    baked->stamp = stamp;
    if (!reader->timeRange(&baked->firstSeconds, &baked->lastSeconds)) {
        baked->firstSeconds = -1e9;  // one sample: every time reads it
        baked->lastSeconds = 1e9;
    }
    baked->read = [reader](double seconds) {
        const StrandSet curves = reader->at(seconds);
        const std::vector<float>* wetness = curves.pointChannel(kWetnessChannel);
        auto guides = std::make_shared<GuideSet>();
        for (std::size_t s = 0; s < curves.strandCount(); ++s) {
            const Vec3f* p = curves.strandPoints(s);
            const std::size_t n = curves.strandPointCount(s);
            Guide& g = guides->add(curves.ids()[s], std::vector<Vec3f>(p, p + n));
            if (!wetness) continue;
            const auto at = wetness->begin() + static_cast<std::ptrdiff_t>(curves.strandBegin(s));
            g.pointAttributes[kWetnessChannel].assign(at, at + static_cast<std::ptrdiff_t>(n));
        }
        return GuidesPtr(guides);
    };
    return baked;
}

}  // namespace groomlab
