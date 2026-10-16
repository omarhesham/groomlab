// SPDX-License-Identifier: Apache-2.0
#pragma once

// The host's space and the groom's (docs/BLENDER.md, "Spaces and units").
//
// A helper session keeps its groom in GroomLab's own space, the one a .groom file is written in
// and Maya opens as it is: Y up, centimeters. The host speaks its own (Blender: Z up, meters times
// the scene's unit scale), and every point, width and length crossing the pipe is turned here, at
// the boundary, never inside the groom: a Z-up point (x, y, z) is the Y-up point (x, z, -y)
// (convertUpAxis' quarter turn about X, exact), times `scale` centimeters per host unit (100 for
// meters), worked in double and rounded once to float. So the graph's lengths are centimeters as in
// Maya, a preset fits the same, and a groom saved from Blender evaluates in Maya to the same bits.

#include <stdexcept>
#include <string>

#include "groomlab/math.h"

namespace groomlab::helper {

struct HostSpace {
    std::string upAxis = "Z";    // the host's up axis: "Z" (Blender) or "Y"
    double metersPerUnit = 1.0;  // the host's unit in meters (Blender: the scene's unit scale)

    double scale() const { return metersPerUnit * 100.0; }  // centimeters per host unit
    bool zUp() const { return upAxis == "Z"; }
    bool identity() const { return !zUp() && scale() == 1.0; }

    Vec3f toGroom(float x, float y, float z) const {
        const double s = scale();
        if (zUp()) return {static_cast<float>(x * s), static_cast<float>(z * s), static_cast<float>(-double(y) * s)};
        return {static_cast<float>(x * s), static_cast<float>(y * s), static_cast<float>(z * s)};
    }
    // Into the host's: x, y, z written to out[0..2].
    void toHost(const Vec3f& p, float* out) const {
        const double s = scale();
        if (zUp()) {
            out[0] = static_cast<float>(p.x / s);
            out[1] = static_cast<float>(-double(p.z) / s);
            out[2] = static_cast<float>(p.y / s);
        } else {
            out[0] = static_cast<float>(p.x / s);
            out[1] = static_cast<float>(p.y / s);
            out[2] = static_cast<float>(p.z / s);
        }
    }
    float lengthToHost(float length) const { return static_cast<float>(length / scale()); }
    // A direction into the host's (turned, not scaled): x, y, z written to out[0..2].
    void directionToHost(const Vec3f& d, float* out) const {
        out[0] = d.x;
        out[1] = zUp() ? -d.z : d.y;
        out[2] = zUp() ? d.y : d.z;
    }

    static HostSpace make(const std::string& upAxis, double metersPerUnit) {
        if (upAxis != "Y" && upAxis != "Z") throw std::invalid_argument("space.upAxis is \"Y\" or \"Z\", not \"" + upAxis + "\"");
        if (!(metersPerUnit > 0.0) || metersPerUnit > 1e6) throw std::invalid_argument("space.metersPerUnit must be above 0");
        HostSpace s;
        s.upAxis = upAxis;
        s.metersPerUnit = metersPerUnit;
        return s;
    }
};

}  // namespace groomlab::helper
