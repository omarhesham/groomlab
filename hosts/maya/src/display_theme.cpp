// SPDX-License-Identifier: Apache-2.0
#include "display_theme.h"
#include "guarded.h"

#include <sstream>
#include <string>

#include <maya/M3dView.h>
#include <maya/MArgDatabase.h>
#include <maya/MGlobal.h>
#include <maya/MString.h>

namespace groomlab::maya {

namespace {

std::vector<float> floats(const MString& text) {
    std::vector<float> out;
    std::istringstream in(text.asChar());
    float v = 0.0f;
    while (in >> v) out.push_back(v);
    return out;
}

const char* kFlags[][2] = {{"-lv", "-levels"},       {"-ps", "-pointSize"},
                           {"-sl", "-selected"},     {"-ss", "-softSelected"}, {"-fz", "-frozen"},
                           {"-pa", "-parts"},        {"-fl", "-flow"},       {"-ff", "-flowField"},
                           {"-tb", "-tubes"},        {"-in", "-instances"},  {"-cd", "-cards"},
                           {"-cc", "-clumpCenters"}, {"-rs", "-reset"}};

}  // namespace

DisplayTheme& displayTheme() {
    static DisplayTheme theme;  // the defaults: primaries blue, then emerald, yellow, red, white down the levels
    return theme;
}

const char* DisplayThemeCmd::kName = "groomLabDisplayTheme";

void* DisplayThemeCmd::creator() { return new DisplayThemeCmd; }

MSyntax DisplayThemeCmd::newSyntax() {
    MSyntax s;
    for (const auto& flag : kFlags) {
        if (std::string(flag[1]) == "-reset") {
            s.addFlag(flag[0], flag[1]);
        } else if (std::string(flag[1]) == "-pointSize") {
            s.addFlag(flag[0], flag[1], MSyntax::kDouble);
        } else {
            s.addFlag(flag[0], flag[1], MSyntax::kString);
        }
    }
    return s;
}

MStatus DisplayThemeCmd::doIt(const MArgList& args) {
    return groomlab::maya::guarded("DisplayThemeCmd::doIt", [&]() -> MStatus {
        MStatus status;
        MArgDatabase db(syntax(), args, &status);
        if (!status) return status;
        DisplayTheme& t = displayTheme();
        const std::uint64_t version = t.version;
        if (db.isFlagSet("-reset")) t = DisplayTheme{};
    
        auto text = [&](const char* flag, MString& out) {
            if (!db.isFlagSet(flag)) return false;
            db.getFlagArgument(flag, 0, out);
            return true;
        };
        auto triples = [&](const char* flag, std::vector<DisplayTheme::RGB>& out) {
            MString v;
            if (!text(flag, v)) return;
            const std::vector<float> f = floats(v);
            out.clear();
            for (std::size_t i = 0; i + 2 < f.size(); i += 3) out.push_back({f[i], f[i + 1], f[i + 2]});
        };
        auto rgb = [&](const char* flag, auto& out) {
            MString v;
            if (!text(flag, v)) return;
            const std::vector<float> f = floats(v);
            for (std::size_t i = 0; i < 3 && i < f.size(); ++i) out[i] = f[i];
        };
        triples("-levels", t.levels);
        if (db.isFlagSet("-pointSize")) {
            double size = 6.0;
            db.getFlagArgument("-pointSize", 0, size);
            t.pointSize = static_cast<float>(size < 0.0 ? 0.0 : size);
        }
        rgb("-selected", t.selected);
        rgb("-softSelected", t.softSelected);
        rgb("-frozen", t.frozen);
        rgb("-parts", t.parts);
        rgb("-flow", t.flow);
        rgb("-flowField", t.flowField);
        rgb("-tubes", t.tubes);
        rgb("-instances", t.instances);
        rgb("-cards", t.cards);
        rgb("-clumpCenters", t.clumpCenters);
        t.version = version + 1;
        if (MGlobal::mayaState() == MGlobal::kInteractive) M3dView::scheduleRefreshAllViews();
        return MS::kSuccess;
    });
}

}  // namespace groomlab::maya
