// SPDX-License-Identifier: Apache-2.0
#include "groomlab/expression.h"
#include "groomlab/portable_math.h"
#include "groomlab/preview_core.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <map>
#include <set>

#include <nlohmann/json.hpp>

#include "groomlab/noise.h"
#include "groomlab/random.h"

// The interpreter's per-operation helpers, inlined into its loop (a call per operation costs a
// quarter of a simple expression's time).
#if defined(_MSC_VER)
#define GROOMLAB_EXPRESSION_INLINE __forceinline
#define GROOMLAB_EXPRESSION_NOINLINE __declspec(noinline)
#else
#define GROOMLAB_EXPRESSION_INLINE inline __attribute__((always_inline))
#define GROOMLAB_EXPRESSION_NOINLINE __attribute__((noinline))
#endif

namespace groomlab {

namespace {

enum Var : std::uint8_t {
    kU, kV, kX, kY, kZ, kNX, kNY, kNZ, kRand, kIndex,
    kLength, kLongest, kReach, kStraightness, kLift, kTipX, kTipY, kTipZ,  // the strand's shape
    kT, kPT, kPX, kPY, kPZ, kPointIndex,                                   // the point (per point)
    kFrame, kTime,                                                         // the evaluated time
    kId, kFaceId, kPrefX, kPrefY, kPrefZ,                                  // XGen's $id, $faceId, $Pref
    kVarCount
};
const char* const kVarNames[] = {"u",      "v",       "x",       "y",     "z",            "nx",   "ny",   "nz",   "rand",
                                 "index",  "length",  "longest", "reach", "straightness", "lift", "tipx", "tipy", "tipz",
                                 "t",      "pt",      "px",      "py",    "pz",           "pointIndex",
                                 "frame",  "time",
                                 "id",     "faceId",  "prefx",   "prefy", "prefz"};
static_assert(sizeof(kVarNames) / sizeof(kVarNames[0]) == kVarCount, "every variable has a name");

// Built-in vectors: three variables each.
struct VectorVar {
    const char* name;
    Var x, y, z;
};
const VectorVar kVectorVars[] = {{"P", kX, kY, kZ},
                                 {"N", kNX, kNY, kNZ},
                                 {"Pref", kPrefX, kPrefY, kPrefZ},
                                 {"tip", kTipX, kTipY, kTipZ},
                                 {"pointP", kPX, kPY, kPZ}};

// XGen's names for GroomLab's variables.
struct Alias {
    const char* from;
    const char* to;
};
const Alias kVariableAliases[] = {{"cLength", "length"}, {"faceid", "faceId"}, {"Pg", "P"},       {"Pw", "P"},
                                  {"Pgw", "P"},          {"Ng", "N"},          {"Prefg", "Pref"}, {"Prefw", "Pref"},
                                  {"Prefgw", "Pref"}};
// XGen's variables GroomLab has nothing for: what to write instead.
const Alias kUnsupportedVariables[] = {
    {"Nref", "XGen's $Nref (the reference normal) has no GroomLab equivalent: $N is the scalp's normal"},
    {"Nrefg", "XGen's $Nrefg (the reference normal) has no GroomLab equivalent: $N is the scalp's normal"},
    {"dPdu", "XGen's surface derivatives ($dPdu, $dPdv) are not available: build a direction from $N, e.g. cross($N, [0, 1, 0])"},
    {"dPdv", "XGen's surface derivatives ($dPdu, $dPdv) are not available: build a direction from $N, e.g. cross($N, [0, 1, 0])"},
    {"dPdug", "XGen's surface derivatives ($dPdu, $dPdv) are not available: build a direction from $N, e.g. cross($N, [0, 1, 0])"},
    {"dPdvg", "XGen's surface derivatives ($dPdu, $dPdv) are not available: build a direction from $N, e.g. cross($N, [0, 1, 0])"},
    {"dPduref", "XGen's surface derivatives ($dPdu, $dPdv) are not available: build a direction from $N, e.g. cross($N, [0, 1, 0])"},
    {"dPdvref", "XGen's surface derivatives ($dPdu, $dPdv) are not available: build a direction from $N, e.g. cross($N, [0, 1, 0])"},
    {"dPdurefg", "XGen's surface derivatives ($dPdu, $dPdv) are not available: build a direction from $N, e.g. cross($N, [0, 1, 0])"},
    {"dPdvrefg", "XGen's surface derivatives ($dPdu, $dPdv) are not available: build a direction from $N, e.g. cross($N, [0, 1, 0])"},
    {"cWidth", "XGen's $cWidth has no GroomLab equivalent: the Width node sets the width, and no variable reads it"},
    {"cDepth", "XGen's $cDepth (a card's depth) has no GroomLab equivalent"},
    {"aCount", "XGen's $aCount (archives) has no GroomLab equivalent"},
    {"descId", "XGen's $descId has no GroomLab equivalent (an expression belongs to one groom)"},
    {"patchId", "XGen's $patchId has no GroomLab equivalent (a groom has one scalp): $faceId is the root's triangle"},
    {"objectId", "XGen's $objectId has no GroomLab equivalent (a groom has one scalp): $faceId is the root's triangle"},
    {"Cs", "XGen's $Cs (the region color) has no GroomLab equivalent: read a painted map with cmap(\"name\")"},
    {"As", "XGen's $As (the region alpha) has no GroomLab equivalent: read a painted map with map(\"name\")"},
    {"DESC", "${DESC} means something only inside an XGen map path, map('${DESC}/paintmaps/name/')"},
};

enum Fn : std::uint8_t {
    // Numbers, component by component on vectors; inlined into the interpreter's loop (classicFn).
    kSin, kCos, kTan, kAsin, kAcos, kAtan, kAbs, kSqrt, kFloor, kCeil, kFract, kExp, kLog, kSign,
    kPow, kAtan2, kMin, kMax, kMod, kStep, kRandFn,
    kClamp, kMix, kSmoothstep, kNoise,
    // Numbers, component by component on vectors (scalarFn).
    kRound, kTrunc, kRadians, kDegrees, kGamma, kBias, kGain, kLerp, kLinearstep, kFit, kRemap,
    kSinh, kCosh, kTanh, kAsinh, kAcosh, kAtanh, kCbrt, kLog10, kFmod, kHypot, kInvert,
    kCosd, kSind, kTand, kAcosd, kAsind, kAtand, kAtan2d,
    kCompress, kExpand, kContrast, kBoxstep, kGaussstep,
    kChoose, kWChoose, kSpline,
    // Numbers only.
    kHash, kPick, kCycle, kRandRange, kNoise4,
    // Vectors, points and numbers by a signature (vectorFn), from kDot on.
    kDot, kCross, kLengthFn, kNormalize, kDistance, kRotate, kAngle, kOrtho, kUp, kIndexFn,
    kRgbToHsl, kHslToRgb, kSaturate, kHsi, kMidHsi,
    kNoiseP, kSNoise, kVNoise, kCNoise, kSNoise4, kVNoise4, kCNoise4,
    kFbm, kVFbm, kCFbm, kTurbulence, kVTurbulence, kCTurbulence, kFbm4, kVFbm4, kCFbm4,
    kCellnoise, kCCellnoise, kPNoise, kVoronoi, kCVoronoi, kPVoronoi,
    // Curves (keys fixed when compiled), maps.
    kCurve, kCCurve, kRampCurve, kLinearCurve, kMap, kCMap,
    kFnCount
};
enum class FnKind : std::uint8_t {
    Math,    // numbers, or vectors component by component
    Scalar,  // numbers only
    Vector,  // a signature: v a vector (a number on every component), p a point (a number x is
             // [x, 0, 0]), s a number; arguments past minArgs at their defaults
    Curve,   // t and constant keys
    Map,     // a map's name
};
struct FnInfo {
    const char* name;  // a name with a space in it can't be written: one the parser picks itself
    int minArgs, maxArgs;
    FnKind kind;
    const char* signature = "";
    int results = 1;
    std::array<float, 7> defaults{};  // Vector: each optional argument's (by its place)
};
constexpr float kOff = 0.0f;
const FnInfo kFns[] = {
    {"sin", 1, 1, FnKind::Math},        {"cos", 1, 1, FnKind::Math},         {"tan", 1, 1, FnKind::Math},
    {"asin", 1, 1, FnKind::Math},       {"acos", 1, 1, FnKind::Math},        {"atan", 1, 1, FnKind::Math},
    {"abs", 1, 1, FnKind::Math},        {"sqrt", 1, 1, FnKind::Math},        {"floor", 1, 1, FnKind::Math},
    {"ceil", 1, 1, FnKind::Math},       {"fract", 1, 1, FnKind::Math},       {"exp", 1, 1, FnKind::Math},
    {"log", 1, 1, FnKind::Math},        {"sign", 1, 1, FnKind::Math},        {"pow", 2, 2, FnKind::Math},
    {"atan2", 2, 2, FnKind::Math},      {"min", 2, 2, FnKind::Math},         {"max", 2, 2, FnKind::Math},
    {"mod", 2, 2, FnKind::Math},        {"step", 2, 2, FnKind::Math},        {"rand", 0, 3, FnKind::Scalar},
    {"clamp", 3, 3, FnKind::Math},      {"mix", 3, 3, FnKind::Math},         {"smoothstep", 3, 3, FnKind::Math},
    {"noise", 1, 4, FnKind::Scalar},
    {"round", 1, 1, FnKind::Math},      {"trunc", 1, 1, FnKind::Math},       {"radians", 1, 1, FnKind::Math},
    {"degrees", 1, 1, FnKind::Math},    {"gamma", 2, 2, FnKind::Math},       {"bias", 2, 2, FnKind::Math},
    {"gain", 2, 2, FnKind::Math},       {"lerp", 3, 3, FnKind::Math},        {"linearstep", 3, 3, FnKind::Math},
    {"fit", 5, 5, FnKind::Math},        {"remap", 5, 5, FnKind::Math},
    {"sinh", 1, 1, FnKind::Math},       {"cosh", 1, 1, FnKind::Math},        {"tanh", 1, 1, FnKind::Math},
    {"asinh", 1, 1, FnKind::Math},      {"acosh", 1, 1, FnKind::Math},       {"atanh", 1, 1, FnKind::Math},
    {"cbrt", 1, 1, FnKind::Math},       {"log10", 1, 1, FnKind::Math},       {"fmod", 2, 2, FnKind::Math},
    {"hypot", 2, 2, FnKind::Math},      {"invert", 1, 1, FnKind::Math},
    {"cosd", 1, 1, FnKind::Math},       {"sind", 1, 1, FnKind::Math},        {"tand", 1, 1, FnKind::Math},
    {"acosd", 1, 1, FnKind::Math},      {"asind", 1, 1, FnKind::Math},       {"atand", 1, 1, FnKind::Math},
    {"atan2d", 2, 2, FnKind::Math},
    {"compress", 3, 3, FnKind::Math},   {"expand", 3, 3, FnKind::Math},      {"contrast", 2, 2, FnKind::Math},
    {"boxstep", 2, 2, FnKind::Math},    {"gaussstep", 3, 3, FnKind::Math},
    {"choose", 2, 64, FnKind::Math},    {"wchoose", 3, 63, FnKind::Math},    {"spline", 5, 64, FnKind::Math},
    {"hash", 1, 64, FnKind::Scalar},    {"pick", 3, 64, FnKind::Scalar},     {"cycle", 3, 3, FnKind::Scalar},
    {"rand range", 2, 3, FnKind::Scalar}, {"noise 4", 4, 4, FnKind::Scalar},
    {"dot", 2, 2, FnKind::Vector, "vv"},
    {"cross", 2, 2, FnKind::Vector, "vv", 3},
    {"length", 1, 1, FnKind::Vector, "v"},
    {"normalize", 1, 1, FnKind::Vector, "v", 3},
    {"distance", 2, 2, FnKind::Vector, "vv"},
    {"rotate", 3, 3, FnKind::Vector, "vvs", 3},
    {"angle", 2, 2, FnKind::Vector, "vv"},
    {"ortho", 2, 2, FnKind::Vector, "vv", 3},
    {"up", 2, 2, FnKind::Vector, "vv", 3},
    {"index of", 2, 2, FnKind::Vector, "vs"},
    {"rgbtohsl", 1, 1, FnKind::Vector, "v", 3},
    {"hsltorgb", 1, 1, FnKind::Vector, "v", 3},
    {"saturate", 2, 2, FnKind::Vector, "vs", 3},
    {"hsi", 4, 5, FnKind::Vector, "vssss", 3, {kOff, kOff, kOff, kOff, 1.0f}},
    {"midhsi", 5, 7, FnKind::Vector, "vssssss", 3, {kOff, kOff, kOff, kOff, kOff, 1.0f, 0.0f}},
    {"noise", 1, 1, FnKind::Vector, "p"},
    {"snoise", 1, 1, FnKind::Vector, "p"},
    {"vnoise", 1, 1, FnKind::Vector, "p", 3},
    {"cnoise", 1, 1, FnKind::Vector, "p", 3},
    {"snoise4", 2, 2, FnKind::Vector, "ps"},
    {"vnoise4", 2, 2, FnKind::Vector, "ps", 3},
    {"cnoise4", 2, 2, FnKind::Vector, "ps", 3},
    {"fbm", 1, 4, FnKind::Vector, "psss", 1, {kOff, 6.0f, 2.0f, 0.5f}},
    {"vfbm", 1, 4, FnKind::Vector, "psss", 3, {kOff, 6.0f, 2.0f, 0.5f}},
    {"cfbm", 1, 4, FnKind::Vector, "psss", 3, {kOff, 6.0f, 2.0f, 0.5f}},
    {"turbulence", 1, 4, FnKind::Vector, "psss", 1, {kOff, 6.0f, 2.0f, 0.5f}},
    {"vturbulence", 1, 4, FnKind::Vector, "psss", 3, {kOff, 6.0f, 2.0f, 0.5f}},
    {"cturbulence", 1, 4, FnKind::Vector, "psss", 3, {kOff, 6.0f, 2.0f, 0.5f}},
    {"fbm4", 2, 5, FnKind::Vector, "pssss", 1, {kOff, kOff, 6.0f, 2.0f, 0.5f}},
    {"vfbm4", 2, 5, FnKind::Vector, "pssss", 3, {kOff, kOff, 6.0f, 2.0f, 0.5f}},
    {"cfbm4", 2, 5, FnKind::Vector, "pssss", 3, {kOff, kOff, 6.0f, 2.0f, 0.5f}},
    {"cellnoise", 1, 1, FnKind::Vector, "p"},
    {"ccellnoise", 1, 1, FnKind::Vector, "p", 3},
    {"pnoise", 2, 2, FnKind::Vector, "pv"},
    {"voronoi", 1, 7, FnKind::Vector, "pssssss", 1, {kOff, 1.0f, 0.5f, 0.0f, 4.0f, 2.0f, 0.5f}},
    {"cvoronoi", 1, 7, FnKind::Vector, "pssssss", 3, {kOff, 1.0f, 0.5f, 0.0f, 4.0f, 2.0f, 0.5f}},
    {"pvoronoi", 1, 6, FnKind::Vector, "psssss", 3, {kOff, 0.5f, 0.0f, 4.0f, 2.0f, 0.5f}},
    {"curve", 1, 64, FnKind::Curve},    {"ccurve", 1, 64, FnKind::Curve, "", 3},
    {"curve pairs", 3, 64, FnKind::Curve}, {"linearcurve", 3, 64, FnKind::Curve},
    {"map", 1, 2, FnKind::Map},         {"cmap", 1, 1, FnKind::Map, "", 3},
};
static_assert(sizeof(kFns) / sizeof(kFns[0]) == kFnCount, "every function has an entry");

// Other names for functions: SeExpr's short ones, XGen's vmap.
const Alias kFunctionAliases[] = {{"deg", "degrees"}, {"rad", "radians"}, {"norm", "normalize"},
                                  {"dist", "distance"}, {"swatch", "choose"}, {"vmap", "cmap"}};
// SeExpr functions GroomLab does not run: why, and what to write instead.
const Alias kUnsupportedFunctions[] = {
    {"printf", "printf is not supported (expressions run on many threads at once): show a value by returning it"},
    {"sprintf", "sprintf is not supported: strings appear only as map names"},
    {"getVar", "getVar is not supported: read the variable by its name ($name)"},
};

const char* aliasOf(const Alias* table, std::size_t count, const std::string& name) {
    for (std::size_t i = 0; i < count; ++i) {
        if (name == table[i].from) return table[i].to;
    }
    return nullptr;
}
template <std::size_t N>
const char* aliasOf(const Alias (&table)[N], const std::string& name) {
    return aliasOf(table, N, name);
}

std::string canonicalFunction(const std::string& name) {
    const char* to = aliasOf(kFunctionAliases, name);
    return to ? std::string(to) : name;
}

enum Bin : std::uint8_t { kAdd, kSub, kMul, kDiv, kPowOp, kLt, kLe, kGt, kGe, kEq, kNe, kFmodOp };
enum Ty : std::uint8_t { kNumber, kVector };

// Locals are compiled in place where they are read, so `$b = $a + $a; $c = $b + $b; ...`
// doubles the program with each line: past this many operations, a clear error.
constexpr std::size_t kMaxProgram = std::size_t{1} << 20;
// Stack slots an evaluation may use (a vector takes three).
constexpr std::size_t kMaxStack = 128;

using Op = Expression::Op;

Op makeOp(Op::Kind kind, std::uint8_t code = 0, float value = 0.0f) {
    Op op{};
    op.kind = kind;
    op.code = code;
    op.value = value;
    return op;
}

// What an evaluation reads: the variables, the strand, its channels and root.
struct EnvData {
    std::array<float, kVarCount> vars{};
    const StrandSet* strands = nullptr;
    std::size_t strand = 0;
    std::size_t point = 0;  // index into the point arrays (the strand's first point unless per point)
    const std::vector<const std::vector<float>*>* channels = nullptr;
    const std::vector<const std::vector<float>*>* pointChannels = nullptr;
    const StrandRoot* root = nullptr;  // where map() reads (null: maps read 0)
};

// The program's tables: curves and maps, by an op's channel.
struct Tables {
    const Ramp* ramps = nullptr;
    const ExpressionCurve* curves = nullptr;
    const ExpressionMapPtr* maps = nullptr;
};

constexpr float kDegToRad = 3.14159265f / 180.0f;
constexpr float kRadToDeg = 180.0f / 3.14159265f;

std::uint32_t floatBits(float f) {
    if (f == 0.0f) f = 0.0f;  // -0 and 0 hash alike
    std::uint32_t bits = 0;
    std::memcpy(&bits, &f, sizeof bits);
    return bits;
}

// A whole number from a float as C's (int) cast makes one (toward zero; NaN: 0), kept within a
// range a loop can walk.
std::int64_t intOf(float f) {
    if (!(f == f)) return 0;
    return static_cast<std::int64_t>(std::trunc(std::clamp(f, -1.0e9f, 1.0e9f)));
}

float biasOf(float x, float b) {
    if (!(x > 0.0f)) return 0.0f;
    if (!(b > 0.0f)) return 0.0f;
    if (b >= 1.0f) return 1.0f;
    return pm::pow(x, pm::log(b) / pm::log(0.5f));
}

// SeExpr's steps, in its (x, a, b) order; a > b runs the other way.
float linearstepX(float x, float a, float b) {
    if (a == b) return x < a ? 0.0f : 1.0f;
    return std::clamp((x - a) / (b - a), 0.0f, 1.0f);
}
float smoothstepX(float x, float a, float b) { return smoothstep(a, b, x); }
float gaussstepX(float x, float a, float b) {
    if (a == b) return x < a ? 0.0f : 1.0f;
    float t = 0.0f;
    if (a < b) {
        if (x < a) return 0.0f;
        if (x >= b) return 1.0f;
        t = 1.0f - (x - a) / (b - a);
    } else {
        if (x <= b) return 1.0f;
        if (x > a) return 0.0f;
        t = (x - b) / (a - b);
    }
    return pm::pow(2.0f, -8.0f * t * t);
}
// SeExpr's remap: 1 within `range` of `source`, falling to 0 over `falloff` beyond it.
float remapX(float x, float source, float range, float falloff, float interp) {
    range = std::abs(range);
    falloff = std::abs(falloff);
    if (falloff == 0.0f) return std::abs(x - source) < range ? 1.0f : 0.0f;
    float a = 0.0f, b = 0.0f;
    if (x > source) {
        a = source + range;
        b = a + falloff;
    } else {
        a = source - range;
        b = a - falloff;
    }
    switch (intOf(interp)) {
        case 0: return linearstepX(x, b, a);
        case 1: return smoothstepX(x, b, a);
        default: return gaussstepX(x, b, a);
    }
}

std::uint64_t seededRandom(float seed, float strandRandom) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &seed, sizeof bits);
    std::uint32_t var = 0;
    std::memcpy(&var, &strandRandom, sizeof var);
    return hashCombine(hash64(var), bits);
}

// The first functions (sin to noise, codes up to kNoise): small, inlined into the interpreter's
// loop as they always were. The rest are called (scalarFn), keeping the loop's frame small.
GROOMLAB_EXPRESSION_INLINE float classicFn(std::uint8_t code, const float* a, const EnvData& env) {
    switch (code) {
        case kSin: return pm::sin(a[0]);
        case kCos: return pm::cos(a[0]);
        case kTan: return pm::tan(a[0]);
        case kAsin: return pm::asin(std::clamp(a[0], -1.0f, 1.0f));
        case kAcos: return pm::acos(std::clamp(a[0], -1.0f, 1.0f));
        case kAtan: return pm::atan(a[0]);
        case kAbs: return std::abs(a[0]);
        case kSqrt: return std::sqrt(std::max(a[0], 0.0f));
        case kFloor: return std::floor(a[0]);
        case kCeil: return std::ceil(a[0]);
        case kFract: return a[0] - std::floor(a[0]);
        case kExp: return pm::exp(a[0]);
        case kLog: return a[0] > 0.0f ? pm::log(a[0]) : 0.0f;
        case kSign: return a[0] > 0.0f ? 1.0f : (a[0] < 0.0f ? -1.0f : 0.0f);
        case kPow: return pm::pow(a[0], a[1]);
        case kAtan2: return pm::atan2(a[0], a[1]);
        case kMin: return std::min(a[0], a[1]);
        case kMax: return std::max(a[0], a[1]);
        case kMod: return a[1] != 0.0f ? a[0] - a[1] * std::floor(a[0] / a[1]) : 0.0f;
        case kStep: return a[1] < a[0] ? 0.0f : 1.0f;
        case kRandFn: return unitFloat(seededRandom(a[0], env.vars[kRand]));
        case kClamp: return std::clamp(a[0], std::min(a[1], a[2]), std::max(a[1], a[2]));
        case kMix: return a[0] + (a[1] - a[0]) * a[2];
        case kSmoothstep: return smoothstep(a[0], a[1], a[2]);  // edges first (the parser orders them)
        case kNoise: return 0.5f + 0.5f * gradientNoise({a[0], a[1], a[2]}, 0x6E01);
        default: return 0.0f;
    }
}

// SeExpr's spline: Catmull-Rom through v[1] .. v[n-2], v[0] and v[n-1] the end controls.
float splineOf(float t, const float* v, std::size_t n) {
    const float u = std::clamp(t, 0.0f, 1.0f);
    if (u == 0.0f) return v[1];
    if (u == 1.0f) return v[n - 2];
    const std::size_t segments = n - 3;
    const float x = u * static_cast<float>(segments);
    const std::size_t seg = std::min(static_cast<std::size_t>(x), segments - 1);
    const float f = x - static_cast<float>(seg);
    const float* p = v + seg;
    const float f2 = f * f, f3 = f2 * f;
    return 0.5f * (p[0] * (-f3 + 2.0f * f2 - f) + p[1] * (3.0f * f3 - 5.0f * f2 + 2.0f) +
                   p[2] * (-3.0f * f3 + 4.0f * f2 + f) + p[3] * (f3 - f2));
}

// A number function of numbers (args: n of them).
GROOMLAB_EXPRESSION_NOINLINE float scalarFn(std::uint8_t code, const float* a, std::size_t n, const EnvData& env) {
    if (code <= kNoise) return classicFn(code, a, env);
    switch (code) {
        case kRound: return std::round(a[0]);
        case kTrunc: return std::trunc(a[0]);
        case kRadians: return a[0] * kDegToRad;
        case kDegrees: return a[0] * kRadToDeg;
        case kGamma: return a[1] > 0.0f && a[0] > 0.0f ? pm::pow(a[0], 1.0f / a[1]) : 0.0f;
        case kBias: return biasOf(a[0], a[1]);
        case kGain:
            return a[0] < 0.5f ? 0.5f * biasOf(2.0f * a[0], 1.0f - a[1]) : 1.0f - 0.5f * biasOf(2.0f - 2.0f * a[0], 1.0f - a[1]);
        case kLerp: return a[0] + (a[1] - a[0]) * a[2];
        case kLinearstep: return linearstepX(a[2], a[0], a[1]);  // edges first (the parser orders them)
        case kFit: return a[2] != a[1] ? a[3] + (a[0] - a[1]) / (a[2] - a[1]) * (a[4] - a[3]) : a[3];
        case kRemap: return remapX(a[0], a[1], a[2], a[3], a[4]);
        case kSinh: return pm::sinh(a[0]);
        case kCosh: return pm::cosh(a[0]);
        case kTanh: return pm::tanh(a[0]);
        case kAsinh: return std::asinh(a[0]);
        case kAcosh: return std::acosh(a[0]);
        case kAtanh: return std::atanh(a[0]);
        case kCbrt: return pm::cbrt(a[0]);
        case kLog10: return a[0] > 0.0f ? pm::log10(a[0]) : 0.0f;
        case kFmod: return a[1] != 0.0f ? std::fmod(a[0], a[1]) : 0.0f;
        case kHypot: return pm::hypot(a[0], a[1]);
        case kInvert: return 1.0f - a[0];
        case kCosd: return pm::cos(a[0] * kDegToRad);
        case kSind: return pm::sin(a[0] * kDegToRad);
        case kTand: return pm::tan(a[0] * kDegToRad);
        case kAcosd: return pm::acos(std::clamp(a[0], -1.0f, 1.0f)) * kRadToDeg;
        case kAsind: return pm::asin(std::clamp(a[0], -1.0f, 1.0f)) * kRadToDeg;
        case kAtand: return pm::atan(a[0]) * kRadToDeg;
        case kAtan2d: return pm::atan2(a[0], a[1]) * kRadToDeg;
        case kCompress: return a[1] + a[0] * (a[2] - a[1]);
        case kExpand:
            if (a[1] == a[2]) return a[0] < a[1] ? 0.0f : 1.0f;
            return (a[0] - a[1]) / (a[2] - a[1]);
        case kContrast:
            return a[0] < 0.5f ? 0.5f * biasOf(1.0f - a[1], 2.0f * a[0]) : 1.0f - 0.5f * biasOf(1.0f - a[1], 2.0f - 2.0f * a[0]);
        case kBoxstep: return a[0] < a[1] ? 0.0f : 1.0f;
        case kGaussstep: return gaussstepX(a[0], a[1], a[2]);
        case kChoose: {
            if (!(a[0] == a[0])) return 0.0f;
            const std::size_t choices = n - 1;
            const float k = std::clamp(a[0] * static_cast<float>(choices), 0.0f, static_cast<float>(choices - 1));
            return a[1 + static_cast<std::size_t>(k)];
        }
        case kWChoose: {
            if (!(a[0] == a[0])) return 0.0f;
            const std::size_t choices = (n - 1) / 2;
            float total = 0.0f;
            for (std::size_t i = 0; i < choices; ++i) total += std::max(a[2 + 2 * i], 0.0f);
            if (!(total > 0.0f)) return a[1];
            const float key = a[0] * total;
            float cutoff = 0.0f;
            std::size_t last = 0;
            for (std::size_t i = 0; i < choices; ++i) {
                const float w = std::max(a[2 + 2 * i], 0.0f);
                if (w <= 0.0f) continue;
                cutoff += w;
                last = i;
                if (key <= cutoff) return a[1 + 2 * i];
            }
            return a[1 + 2 * last];
        }
        case kSpline: return splineOf(a[0], a + 1, n - 1);
        case kHash: {
            std::uint64_t h = hash64(0x4A54);
            for (std::size_t i = 0; i < n; ++i) h = hashCombine(h, floatBits(a[i]));
            return unitFloat(h);
        }
        case kPick: {
            const std::int64_t lo = intOf(a[1]), hi = intOf(a[2]);
            const std::int64_t count = hi - lo + 1;
            if (count <= 0) return static_cast<float>(lo);
            const float h = unitFloat(hashCombine(hash64(0x91C4), floatBits(a[0])));
            const std::size_t weights = n > 3 ? n - 3 : 0;
            if (weights == 0) return static_cast<float>(lo + std::min<std::int64_t>(static_cast<std::int64_t>(h * static_cast<float>(count)), count - 1));
            // A weight per number from lo (missing: 1).
            float total = 0.0f;
            for (std::size_t k = 0; k < weights && static_cast<std::int64_t>(k) < count; ++k) total += std::max(a[3 + k], 0.0f);
            const std::int64_t rest = std::max<std::int64_t>(count - static_cast<std::int64_t>(weights), 0);
            total += static_cast<float>(rest);
            if (!(total > 0.0f)) return static_cast<float>(lo);
            float r = h * total;
            for (std::size_t k = 0; k < weights && static_cast<std::int64_t>(k) < count; ++k) {
                const float w = std::max(a[3 + k], 0.0f);
                if (r < w) return static_cast<float>(lo + static_cast<std::int64_t>(k));
                r -= w;
            }
            if (rest == 0) return static_cast<float>(hi);
            return static_cast<float>(lo + static_cast<std::int64_t>(weights) + std::min<std::int64_t>(static_cast<std::int64_t>(r), rest - 1));
        }
        case kCycle: {
            const std::int64_t lo = intOf(a[1]), hi = intOf(a[2]);
            const std::int64_t span = hi - lo + 1;
            if (span <= 0) return static_cast<float>(lo);
            std::int64_t r = intOf(a[0]) % span;
            if (r < 0) r += span;
            return static_cast<float>(lo + r);
        }
        case kRandRange: {
            const float u = n > 2 ? unitFloat(seededRandom(a[2], env.vars[kRand])) : env.vars[kRand];
            return a[0] + (a[1] - a[0]) * u;
        }
        case kNoise4: return 0.5f + 0.5f * fractalNoise4({a[0], a[1], a[2]}, a[3], 0x6E04, 1);
        default: return 0.0f;
    }
}

std::uint64_t cellHash(float fx, float fy, float fz, std::uint64_t seed) {
    const auto ix = static_cast<std::int64_t>(fx), iy = static_cast<std::int64_t>(fy), iz = static_cast<std::int64_t>(fz);
    std::uint64_t h = hashCombine(seed, static_cast<std::uint64_t>(ix));
    h = hashCombine(h, static_cast<std::uint64_t>(iy));
    return hashCombine(h, static_cast<std::uint64_t>(iz));
}

// Lattice coordinates kept where a float holds whole numbers exactly (and an int64 holds them).
float latticeFloor(float v) { return std::floor(std::clamp(v, -1.0e7f, 1.0e7f)); }

Vec3f cellVector(float cx, float cy, float cz, std::uint64_t seed) {
    const std::uint64_t h = cellHash(cx, cy, cz, seed);
    return {unitFloat(hash64(h + 1)), unitFloat(hash64(h + 2)), unitFloat(hash64(h + 3))};
}

int octavesOf(float octaves) { return static_cast<int>(std::clamp(octaves, 1.0f, 8.0f)); }

// SeExpr's FBM: a sum of octaves, each `lacunarity` times the frequency and `gain` times the
// amplitude of the one before, not normalized (|noise| for turbulence).
float fractalSum(const Vec3f& p, float octaves, float lacunarity, float gain, bool turbulent) {
    const int count = octavesOf(octaves);
    float frequency = 1.0f, amplitude = 1.0f, sum = 0.0f;
    for (int i = 0; i < count; ++i) {
        const float n = gradientNoise(p * frequency, i == 0 ? 0x6E01u : 0xFB00u + static_cast<std::uint64_t>(i));
        sum += amplitude * (turbulent ? std::abs(n) : n);
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return sum;
}

Vec3f vectorFractalSum(const Vec3f& p, float octaves, float lacunarity, float gain, bool turbulent) {
    const int count = octavesOf(octaves);
    float frequency = 1.0f, amplitude = 1.0f;
    Vec3f sum{};
    for (int i = 0; i < count; ++i) {
        Vec3f n = vectorNoise(p * frequency, i == 0 ? 0x6E02u : 0xFC00u + static_cast<std::uint64_t>(i), 1);
        if (turbulent) n = {std::abs(n.x), std::abs(n.y), std::abs(n.z)};
        sum += n * amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return sum;
}

float fractalSum4(const Vec3f& p, float w, float octaves, float lacunarity, float gain) {
    const int count = octavesOf(octaves);
    float frequency = 1.0f, amplitude = 1.0f, sum = 0.0f;
    for (int i = 0; i < count; ++i) {
        sum += amplitude * fractalNoise4(p * frequency, w * frequency, i == 0 ? 0x6E04u : 0xFD00u + static_cast<std::uint64_t>(i), 1);
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return sum;
}

Vec3f vectorFractalSum4(const Vec3f& p, float w, float octaves, float lacunarity, float gain) {
    const int count = octavesOf(octaves);
    float frequency = 1.0f, amplitude = 1.0f;
    Vec3f sum{};
    for (int i = 0; i < count; ++i) {
        sum += vectorNoise4(p * frequency, w * frequency, i == 0 ? 0x6E05u : 0xFE00u + static_cast<std::uint64_t>(i), 1) * amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return sum;
}

Vec3f half(const Vec3f& v) { return v * 0.5f + Vec3f{0.5f, 0.5f, 0.5f}; }

// Gradient noise repeating every `period` cells along each axis (pnoise), about -1 to 1.
float periodicNoise(const Vec3f& p, const Vec3f& period) {
    static constexpr float kGradients[12][3] = {{1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {1, 0, 1}, {-1, 0, 1},
                                                {1, 0, -1}, {-1, 0, -1}, {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}};
    const std::int64_t px = std::max<std::int64_t>(1, intOf(period.x)), py = std::max<std::int64_t>(1, intOf(period.y)),
                       pz = std::max<std::int64_t>(1, intOf(period.z));
    const float fx = latticeFloor(p.x), fy = latticeFloor(p.y), fz = latticeFloor(p.z);
    const auto ix = static_cast<std::int64_t>(fx), iy = static_cast<std::int64_t>(fy), iz = static_cast<std::int64_t>(fz);
    const float dx = p.x - fx, dy = p.y - fy, dz = p.z - fz;
    auto fade = [](float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); };
    auto wrap = [](std::int64_t i, std::int64_t n) { return ((i % n) + n) % n; };
    float corner[8];
    for (int c = 0; c < 8; ++c) {
        const int ox = c & 1, oy = (c >> 1) & 1, oz = (c >> 2) & 1;
        const std::uint64_t h = cellHash(static_cast<float>(wrap(ix + ox, px)), static_cast<float>(wrap(iy + oy, py)),
                                         static_cast<float>(wrap(iz + oz, pz)), 0x9E01);
        const float* g = kGradients[h % 12u];
        corner[c] = g[0] * (dx - static_cast<float>(ox)) + g[1] * (dy - static_cast<float>(oy)) + g[2] * (dz - static_cast<float>(oz));
    }
    const float u = fade(dx), v = fade(dy), w = fade(dz);
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    return lerp(lerp(lerp(corner[0], corner[1], u), lerp(corner[2], corner[3], u), v),
                lerp(lerp(corner[4], corner[5], u), lerp(corner[6], corner[7], u), v), w);
}

// SeExpr's voronoi search: a feature point per unit cell, its middle moved by up to `jitter` / 2;
// the nearest two to p.
struct VoronoiHit {
    float f1 = 0.0f, f2 = 0.0f;  // distances
    Vec3f pos1, pos2;            // the points
    Vec3f cell1;                 // the nearest point's cell (its lattice corner)
};
VoronoiHit voronoiSearch(const Vec3f& p, float jitter) {
    jitter = std::clamp(jitter, 1e-3f, 1.0f);
    const float cx = latticeFloor(p.x), cy = latticeFloor(p.y), cz = latticeFloor(p.z);
    float d1 = 1e30f, d2 = 1e30f;  // squared distances
    VoronoiHit hit;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const float x = cx + static_cast<float>(dx), y = cy + static_cast<float>(dy), z = cz + static_cast<float>(dz);
                const std::uint64_t h = cellHash(x, y, z, 0x7090);
                const Vec3f feature{x + 0.5f + jitter * (unitFloat(hash64(h + 1)) - 0.5f),
                                    y + 0.5f + jitter * (unitFloat(hash64(h + 2)) - 0.5f),
                                    z + 0.5f + jitter * (unitFloat(hash64(h + 3)) - 0.5f)};
                const Vec3f d = p - feature;
                const float d2Here = d.dot(d);
                if (d2Here < d1) {
                    d2 = d1;
                    hit.pos2 = hit.pos1;
                    d1 = d2Here;
                    hit.pos1 = feature;
                    hit.cell1 = {x, y, z};
                } else if (d2Here < d2) {
                    d2 = d2Here;
                    hit.pos2 = feature;
                }
            }
        }
    }
    hit.f1 = std::sqrt(d1);
    hit.f2 = std::sqrt(d2);
    return hit;
}

// voronoi's point after its optional fbm warp (arguments 3-6 of voronoi and cvoronoi).
Vec3f voronoiPoint(const Vec3f& p, float scale, float octaves, float lacunarity, float gain) {
    return scale > 0.0f ? p + vectorFractalSum(p, octaves, lacunarity, gain, false) * scale : p;
}

float voronoiValue(const VoronoiHit& hit, const Vec3f& p, int type) {
    switch (type) {
        case 1: return unitFloat(cellHash(hit.cell1.x, hit.cell1.y, hit.cell1.z, 0xCE12));
        case 2: return hit.f1;
        case 3: return hit.f2;
        case 4: return hit.f2 - hit.f1;
        case 5: {
            const float spread = (hit.pos1 - p).length() + (hit.pos2 - p).length();
            const float sf = spread > 1e-12f ? (hit.pos2 - hit.pos1).length() / spread : 0.0f;
            return smoothstepX(hit.f2 - hit.f1, 0.0f, 0.1f * sf);
        }
        default: return 0.0f;
    }
}

// SeExpr's hue, saturation, lightness (hue 0-1).
Vec3f rgbToHsl(const Vec3f& c) {
    const float mn = std::min({c.x, c.y, c.z}), mx = std::max({c.x, c.y, c.z});
    const float sum = mn + mx, diff = mx - mn, l = sum * 0.5f;
    if (diff < 1e-6f) return {0.0f, 0.0f, l};
    float s = 0.0f;
    if (l <= 0.5f) s = mn < 0.0f ? 1.0f - mn : diff / sum;
    else s = mx > 1.0f ? mx : diff / (2.0f - sum);
    float h = 0.0f;
    if (c.x == mx) h = (c.y - c.z) / diff;
    else if (c.y == mx) h = (c.z - c.x) / diff + 2.0f;
    else h = (c.x - c.y) / diff + 4.0f;
    h /= 6.0f;
    h -= std::floor(h);
    return {h, s, l};
}
float hslValue(float x, float y, float h) {
    h -= std::floor(h);
    if (h < 1.0f / 6.0f) return x + (y - x) * h * 6.0f;
    if (h < 3.0f / 6.0f) return y;
    if (h < 4.0f / 6.0f) return x + (y - x) * (4.0f / 6.0f - h) * 6.0f;
    return x;
}
Vec3f hslToRgb(const Vec3f& hsl) {
    const float h = hsl.x, s = hsl.y, l = hsl.z;
    if (s <= 0.0f) return {l, l, l};
    float y = 0.0f;
    if (l < 0.5f) y = s > 1.0f ? 2.0f * l + s - 1.0f : l + l * s;
    else y = s > 1.0f ? s : l + s - l * s;
    const float x = 2.0f * l - y;
    return {hslValue(x, y, h + 1.0f / 3.0f), hslValue(x, y, h), hslValue(x, y, h - 1.0f / 3.0f)};
}
Vec3f hsiAdjust(const Vec3f& rgb, float h, float s, float i) {
    Vec3f hsl = rgbToHsl(rgb);
    hsl.x += h / 360.0f;
    hsl.y *= s;
    return hslToRgb(hsl) * i;
}

// Rodrigues: v turned by `angle` (radians) around the unit `axis`.
Vec3f rotated(const Vec3f& v, const Vec3f& axis, float angle) {
    const float c = pm::cos(angle), s = pm::sin(angle);
    return v * c + axis.cross(v) * s + axis * (axis.dot(v) * (1.0f - c));
}
float angleOf(const Vec3f& a, const Vec3f& b) {
    const float l = a.length() * b.length();
    return l > 0.0f ? pm::acos(std::clamp(a.dot(b) / l, -1.0f, 1.0f)) : 0.0f;
}
Vec3f unit(const Vec3f& v) {
    const float l = v.length();
    return l > 1e-12f ? v * (1.0f / l) : Vec3f{};
}

// A function of vectors, a point's noise, a curve or a map (Call ops of those codes): reads its
// arguments from `a` and writes its results there. Returns the slots it wrote.
GROOMLAB_EXPRESSION_NOINLINE std::size_t vectorFn(const Op& op, float* a, const EnvData& env, const Tables& tables) {
    auto vec = [&](std::size_t at) { return Vec3f{a[at], a[at + 1], a[at + 2]}; };
    auto put = [&](const Vec3f& v) {
        a[0] = v.x;
        a[1] = v.y;
        a[2] = v.z;
        return std::size_t{3};
    };
    auto one = [&](float v) {
        a[0] = v;
        return std::size_t{1};
    };
    switch (op.code) {
        case kDot: return one(vec(0).dot(vec(3)));
        case kCross: return put(vec(0).cross(vec(3)));
        case kLengthFn: return one(vec(0).length());
        case kNormalize: return put(unit(vec(0)));
        case kDistance: return one((vec(0) - vec(3)).length());
        case kRotate: {
            const Vec3f v = vec(0), k = vec(3);
            if (!(k.length() > 1e-12f)) return put(v);
            return put(rotated(v, unit(k), a[6]));
        }
        case kAngle: return one(angleOf(vec(0), vec(3)));
        case kOrtho: return put(unit(vec(0).cross(vec(3))));
        case kUp: {  // P turned so that +Y points along upvec
            const Vec3f p = vec(0), up = vec(3), y{0.0f, 1.0f, 0.0f};
            if (!(up.length() > 1e-12f)) return put(p);
            const Vec3f axis = y.cross(up);
            if (!(axis.length() > 1e-6f * up.length())) return put(up.y >= 0.0f ? p : Vec3f{p.x, -p.y, -p.z});
            return put(rotated(p, unit(axis), angleOf(y, up)));
        }
        case kIndexFn: {
            const std::int64_t i = intOf(a[3]);
            return one(i >= 0 && i < 3 ? a[i] : 0.0f);
        }
        case kRgbToHsl: return put(rgbToHsl(vec(0)));
        case kHslToRgb: return put(hslToRgb(vec(0)));
        case kSaturate: {
            const Vec3f c = vec(0);
            const float amount = a[3];
            const float lum = c.dot(Vec3f{0.2126f, 0.7152f, 0.0722f});
            const Vec3f r = Vec3f{lum, lum, lum} * (1.0f - amount) + c * amount;
            return put({std::max(r.x, 0.0f), std::max(r.y, 0.0f), std::max(r.z, 0.0f)});
        }
        case kHsi: {
            const float m = a[6];  // the mask
            return put(hsiAdjust(vec(0), a[3] * m, (a[4] - 1.0f) * m + 1.0f, (a[5] - 1.0f) * m + 1.0f));
        }
        case kMidHsi: {
            float m = 2.0f * a[6] - 1.0f;
            m = m < 0.0f ? -remapX(-m, 1.0f, 0.0f, a[7], a[8]) : remapX(m, 1.0f, 0.0f, a[7], a[8]);
            const float h = a[3] * m, w = std::abs(m);
            float s = a[4] * w + 1.0f - w, i = a[5] * w + 1.0f - w;
            if (m < 0.0f) {
                s = s != 0.0f ? 1.0f / s : 0.0f;
                i = i != 0.0f ? 1.0f / i : 0.0f;
            }
            return put(hsiAdjust(vec(0), h, s, i));
        }
        case kNoiseP: return one(0.5f + 0.5f * gradientNoise(vec(0), 0x6E01));
        case kSNoise: return one(gradientNoise(vec(0), 0x6E01));
        case kVNoise: return put(vectorNoise(vec(0), 0x6E02, 1));
        case kCNoise: return put(half(vectorNoise(vec(0), 0x6E02, 1)));
        case kSNoise4: return one(fractalNoise4(vec(0), a[3], 0x6E04, 1));
        case kVNoise4: return put(vectorNoise4(vec(0), a[3], 0x6E05, 1));
        case kCNoise4: return put(half(vectorNoise4(vec(0), a[3], 0x6E05, 1)));
        case kFbm: return one(0.5f + 0.5f * fractalSum(vec(0), a[3], a[4], a[5], false));
        case kVFbm: return put(vectorFractalSum(vec(0), a[3], a[4], a[5], false));
        case kCFbm: return put(half(vectorFractalSum(vec(0), a[3], a[4], a[5], false)));
        case kTurbulence: return one(0.5f + 0.5f * fractalSum(vec(0), a[3], a[4], a[5], true));
        case kVTurbulence: return put(vectorFractalSum(vec(0), a[3], a[4], a[5], true));
        case kCTurbulence: return put(half(vectorFractalSum(vec(0), a[3], a[4], a[5], true)));
        case kFbm4: return one(0.5f + 0.5f * fractalSum4(vec(0), a[3], a[4], a[5], a[6]));
        case kVFbm4: return put(vectorFractalSum4(vec(0), a[3], a[4], a[5], a[6]));
        case kCFbm4: return put(half(vectorFractalSum4(vec(0), a[3], a[4], a[5], a[6])));
        case kCellnoise: return one(unitFloat(cellHash(latticeFloor(a[0]), latticeFloor(a[1]), latticeFloor(a[2]), 0xCE11)));
        case kCCellnoise: return put(cellVector(latticeFloor(a[0]), latticeFloor(a[1]), latticeFloor(a[2]), 0xCE21));
        case kPNoise: return one(periodicNoise(vec(0), vec(3)));
        case kVoronoi:
        case kCVoronoi: {
            const Vec3f p = voronoiPoint(vec(0), a[5], a[6], a[7], a[8]);
            const VoronoiHit hit = voronoiSearch(p, a[4]);
            const int type = static_cast<int>(intOf(a[3]));
            const float value = voronoiValue(hit, p, type);
            if (op.code == kVoronoi) return one(value);
            const Vec3f color = cellVector(hit.cell1.x, hit.cell1.y, hit.cell1.z, 0xC0C1);
            if (type == 1) return put(color);
            return put(type >= 2 && type <= 5 ? color * value : Vec3f{});
        }
        case kPVoronoi: {
            const Vec3f p = voronoiPoint(vec(0), a[4], a[5], a[6], a[7]);
            return put(voronoiSearch(p, a[3]).pos1);
        }
        case kCurve: return one(tables.curves[op.channel](a[0]).x);
        case kCCurve: return put(tables.curves[op.channel](a[0]));
        case kRampCurve:
        case kLinearCurve: return one(tables.ramps[op.channel](a[0]));
        case kMap:
        case kCMap: {
            const ExpressionMap* map = tables.maps[op.channel].get();
            if (op.code == kMap) {
                return one(map && env.root ? map->at(*env.root, static_cast<int>(op.value)) : 0.0f);
            }
            if (!map || !env.root) return put(Vec3f{});
            return put(Vec3f{map->at(*env.root, 0), map->at(*env.root, 1), map->at(*env.root, 2)});
        }
        default: return one(0.0f);
    }
}

bool isScalarFn(std::uint8_t code) { return code < kDot; }

float finite(float v) { return std::isfinite(v) ? v : 0.0f; }

// A number function on every component of its vector arguments (VCall): reads them from `a` and
// writes the vector there.
GROOMLAB_EXPRESSION_NOINLINE void componentwise(std::uint8_t code, std::size_t slots, float* a, const EnvData& env) {
    const std::size_t n = slots / 3u;
    float r[3];
    for (std::size_t k = 0; k < 3; ++k) {
        float args[64];
        for (std::size_t i = 0; i < n && i < 64; ++i) args[i] = a[i * 3 + k];
        r[k] = finite(scalarFn(code, args, n, env));
    }
    a[0] = r[0];
    a[1] = r[1];
    a[2] = r[2];
}

GROOMLAB_EXPRESSION_INLINE float binary(std::uint8_t code, float a, float b) {
    switch (code) {
        case kAdd: return a + b;
        case kSub: return a - b;
        case kMul: return a * b;
        case kDiv: return b != 0.0f ? a / b : 0.0f;
        case kPowOp: return pm::pow(a, b);
        case kLt: return a < b ? 1.0f : 0.0f;
        case kLe: return a <= b ? 1.0f : 0.0f;
        case kGt: return a > b ? 1.0f : 0.0f;
        case kGe: return a >= b ? 1.0f : 0.0f;
        case kEq: return a == b ? 1.0f : 0.0f;
        case kNe: return a != b ? 1.0f : 0.0f;
        case kFmodOp: return b != 0.0f ? std::fmod(a, b) : 0.0f;
    }
    return 0.0f;
}

// Runs [begin, end) (its stack depth checked when parsed); returns the slots left on the stack,
// and in `last` its top (a number: last[0]) or its top three (a vector: last[0..2]). The stack is
// local: nothing the program writes may alias what it reads (env).
template <bool Vector>
GROOMLAB_EXPRESSION_INLINE std::size_t execOps(const Op* begin, const Op* end, const EnvData& env, const Tables& tables,
                                               float* last) {
    float stack[kMaxStack];  // not cleared: every slot is written before it is read
    std::size_t top = 0;
    for (const Op* it = begin; it != end; ++it) {
        const Op& op = *it;
        switch (op.kind) {
            case Op::Number: stack[top++] = op.value; break;
            case Op::Variable: stack[top++] = env.vars[op.code]; break;
            case Op::Channel: {
                const std::vector<float>* ch = env.channels ? (*env.channels)[op.channel] : nullptr;
                if (ch) {
                    stack[top++] = env.strand < ch->size() ? (*ch)[env.strand] : 0.0f;
                    break;
                }
                const std::vector<float>* pc = env.pointChannels ? (*env.pointChannels)[op.channel] : nullptr;
                stack[top++] = pc && env.point < pc->size() ? (*pc)[env.point] : 0.0f;
                break;
            }
            case Op::Unary: stack[top - 1] = -stack[top - 1]; break;
            case Op::Binary: {
                const float b = stack[--top];
                float& a = stack[top - 1];
                a = binary(op.code, a, b);
                break;
            }
            case Op::Select: {
                const float no = stack[--top];
                const float yes = stack[--top];
                float& c = stack[top - 1];
                c = c != 0.0f ? yes : no;
                break;
            }
            case Op::Call: {
                top -= op.args;
                float* a = stack + top;
                if (op.code <= kNoise) {
                    stack[top++] = finite(classicFn(op.code, a, env));
                } else if (isScalarFn(op.code)) {
                    stack[top++] = finite(scalarFn(op.code, a, op.args, env));
                } else {
                    const std::size_t made = vectorFn(op, a, env, tables);
                    for (std::size_t k = 0; k < made; ++k) a[k] = finite(a[k]);
                    top += made;
                }
                break;
            }
            case Op::VCall: {
                top -= op.args;
                componentwise(op.code, op.args, stack + top, env);
                top += 3;
                break;
            }
            case Op::VUnary:
                for (std::size_t k = 1; k <= 3; ++k) stack[top - k] = -stack[top - k];
                break;
            case Op::VBinary: {
                const std::size_t rs = (op.args & 2) ? 3 : 1, ls = (op.args & 1) ? 3 : 1;
                top -= rs;
                const float* b = stack + top;
                top -= ls;
                float* a = stack + top;
                float r[3];
                for (std::size_t k = 0; k < 3; ++k) r[k] = binary(op.code, a[ls == 3 ? k : 0], b[rs == 3 ? k : 0]);
                a[0] = r[0];
                a[1] = r[1];
                a[2] = r[2];
                top += 3;
                break;
            }
            case Op::VSelect: {
                top -= 7;
                float* c = stack + top;
                const float* pick = c[0] != 0.0f ? c + 1 : c + 4;
                const float r[3] = {pick[0], pick[1], pick[2]};
                c[0] = r[0];
                c[1] = r[1];
                c[2] = r[2];
                top += 3;
                break;
            }
            case Op::Component: {
                const float v = stack[top - 3 + op.code];
                top -= 3;
                stack[top++] = v;
                break;
            }
            case Op::Splat: {
                const float v = stack[top - 1];
                stack[top] = v;
                stack[top + 1] = v;
                top += 2;
                break;
            }
        }
    }
    if constexpr (Vector) {
        if (top >= 3) {
            last[0] = stack[top - 3];
            last[1] = stack[top - 2];
            last[2] = stack[top - 1];
        }
    } else {
        if (top >= 1) last[0] = stack[top - 1];
    }
    return top;
}

// Stack slots after op, from those before (the args slots read, the results written).
std::size_t depthAfter(const Op& op, std::size_t depth) {
    switch (op.kind) {
        case Op::Number:
        case Op::Variable:
        case Op::Channel: return depth + 1;
        case Op::Unary:
        case Op::VUnary: return depth;
        case Op::Binary: return depth - 1;
        case Op::VBinary: return depth - ((op.args & 1) ? 3u : 1u) - ((op.args & 2) ? 3u : 1u) + 3u;
        case Op::Call:
        case Op::VCall: return depth - op.args + op.results;
        case Op::Select: return depth - 2;
        case Op::VSelect: return depth - 4;
        case Op::Component: return depth - 2;
        case Op::Splat: return depth + 2;
    }
    return depth;
}

// Recursive descent into reverse Polish, typed: each part is a number or a vector.
class Parser {
public:
    Parser(const std::string& text, std::vector<Op>& out, std::vector<std::string>& channels,
           const std::vector<std::pair<std::string, float>>& constants, std::vector<Ramp>& ramps,
           std::vector<ExpressionCurve>& curves, std::vector<std::string>& mapNames, std::vector<ExpressionMapPtr>& maps,
           const ExpressionEnvironment* environment)
        : s_(text),
          out_(&out),
          channels_(channels),
          constants_(constants),
          ramps_(ramps),
          curves_(curves),
          mapNames_(mapNames),
          maps_(maps),
          environment_(environment) {}

    // A block: statements (`$name = expression;`, `$name += ...;`, if/else of them) making locals
    // usable below them, then the expression whose value is the result (a closing `;` is
    // optional). Returns its type.
    Ty parse() {
        statements(false);
        const Ty type = expression();
        std::size_t end = pos_;  // back over what the last token's lookahead skipped
        while (end > 0 && std::isspace(static_cast<unsigned char>(s_[end - 1]))) --end;
        accept(";");
        skipSpace();
        if (pos_ != s_.size()) {
            // More on a later line: most likely a line above without its `;`.
            if (s_.find('\n', end) < pos_) fail("expected ';' at the end of the line above (only the last line is the value)");
            fail("unexpected '" + std::string(1, s_[pos_]) + "'");
        }
        return type;
    }

    [[noreturn]] void fail(const std::string& what) const {
        // Line and column in a block of several lines, else the position.
        std::size_t line = 1, column = 1;
        for (std::size_t i = 0; i < pos_ && i < s_.size(); ++i) {
            if (s_[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        const bool block = s_.find('\n') != std::string::npos;
        throw ExpressionError(block ? what + " (line " + std::to_string(line) + ", column " + std::to_string(column) + ")"
                                    : "expression '" + s_ + "': " + what + " at position " + std::to_string(pos_ + 1));
    }

private:
    struct Local {
        std::vector<Op> ops;
        Ty type = kNumber;
        std::string error;  // reading it is this error (a name an if set in one branch only)
    };
    // The first argument of a call already compiled: x in x->f(a).
    struct Applied {
        std::size_t start;
        Ty type;
    };

    // Spaces, line breaks and comments (# or // to the end of the line).
    void skipSpace() {
        for (;;) {
            while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
            const bool hash = pos_ < s_.size() && s_[pos_] == '#';
            const bool slashes = pos_ + 1 < s_.size() && s_[pos_] == '/' && s_[pos_ + 1] == '/';
            if (!hash && !slashes) return;
            while (pos_ < s_.size() && s_[pos_] != '\n') ++pos_;
        }
    }
    bool peek(char c) {
        skipSpace();
        return pos_ < s_.size() && s_[pos_] == c;
    }
    // A name, with or without its `$`, and a vector's component (`$dir.x`). False, and
    // nothing read, when there is none here.
    bool readName(std::string& name) {
        skipSpace();
        std::size_t p = pos_;
        if (p < s_.size() && s_[p] == '$') ++p;
        if (p >= s_.size() || !(std::isalpha(static_cast<unsigned char>(s_[p])) || s_[p] == '_')) return false;
        const std::size_t start = p;
        while (p < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[p])) || s_[p] == '_')) ++p;
        if (p + 1 < s_.size() && s_[p] == '.' && std::strchr("xyzrgb", s_[p + 1]) &&
            (p + 2 >= s_.size() || !(std::isalnum(static_cast<unsigned char>(s_[p + 2])) || s_[p + 2] == '_'))) {
            p += 2;
        }
        name = s_.substr(start, p - start);
        pos_ = p;
        return true;
    }
    // `=` (not `==`) after a name: an assignment.
    bool assignment() {
        skipSpace();
        if (pos_ < s_.size() && s_[pos_] == '=' && (pos_ + 1 >= s_.size() || s_[pos_ + 1] != '=')) {
            ++pos_;
            return true;
        }
        return false;
    }
    // += -= *= /= %= ^= after a name.
    bool compoundAssignment(Bin* op) {
        skipSpace();
        if (pos_ + 1 >= s_.size() || s_[pos_ + 1] != '=') return false;
        if (pos_ + 2 < s_.size() && s_[pos_ + 2] == '=') return false;
        switch (s_[pos_]) {
            case '+': *op = kAdd; break;
            case '-': *op = kSub; break;
            case '*': *op = kMul; break;
            case '/': *op = kDiv; break;
            case '%': *op = kFmodOp; break;
            case '^': *op = kPowOp; break;
            default: return false;
        }
        pos_ += 2;
        return true;
    }
    bool accept(const char* token) {
        skipSpace();
        const std::size_t n = std::char_traits<char>::length(token);
        if (s_.compare(pos_, n, token) != 0) return false;
        pos_ += n;
        return true;
    }
    // A minus sign, not the -> of an apply.
    bool acceptMinus() {
        skipSpace();
        if (pos_ < s_.size() && s_[pos_] == '-' && (pos_ + 1 >= s_.size() || s_[pos_ + 1] != '>')) {
            ++pos_;
            return true;
        }
        return false;
    }
    void emit(Op op) { out_->push_back(op); }
    void insertAt(std::size_t at, Op op) { out_->insert(out_->begin() + static_cast<std::ptrdiff_t>(at), op); }
    // a OP b of these types: numbers as ever; a vector on either side, component by component.
    Ty arithmetic(Bin b, Ty left, Ty right) {
        if (left == kNumber && right == kNumber) {
            emit(makeOp(Op::Binary, b));
            return kNumber;
        }
        Op op = makeOp(Op::VBinary, b);
        op.args = static_cast<std::uint8_t>((left == kVector ? 1 : 0) | (right == kVector ? 2 : 0));
        emit(op);
        return kVector;
    }

    // Each nesting level (parentheses, unary signs) recurses: past this, a clear error, not a stack overflow.
    struct Deeper {
        Parser& parser;
        explicit Deeper(Parser& p) : parser(p) {
            if (++parser.depth_ > 200) parser.fail("nested too deeply");
        }
        ~Deeper() { --parser.depth_; }
    };

    // Statements until the first that is not one (a block's own: until its `}`).
    void statements(bool block) {
        for (;;) {
            if (block && peek('}')) return;
            if (!statement()) {
                if (block) fail("expected an assignment ($name = ...;) or an if inside { }");
                return;
            }
        }
    }
    // One statement, or false (and nothing read) when there is none here.
    bool statement() {
        const Deeper deeper(*this);
        skipSpace();
        const std::size_t start = pos_;
        std::string name;
        if (!readName(name)) return false;
        if (name == "else" && peek('{')) fail("an else without its if");
        if (name == "if" && peek('(')) {
            ifStatement();
            return true;
        }
        Bin op = kAdd;
        bool compound = false;
        if (!assignment()) {
            if (!compoundAssignment(&op)) {
                pos_ = start;
                return false;
            }
            compound = true;
        }
        if (name.find('.') != std::string::npos) fail("assign a whole vector ($v = [x, y, z];), not one component");
        Local local;
        std::vector<Op>* outer = out_;
        out_ = &local.ops;
        if (compound) {
            const Ty left = named(name);
            const Ty right = expression();
            local.type = arithmetic(op, left, right);
        } else {
            local.type = expression();
        }
        out_ = outer;
        if (!accept(";")) fail("expected ';' after the value of $" + name);
        locals_[name] = std::move(local);
        if (assigned_) assigned_->insert(name);
        return true;
    }
    // { statements }
    void block() {
        if (!accept("{")) fail("expected '{': an if's statements are in braces, if (c) { $a = 1; }");
        statements(true);
        if (!accept("}")) fail("expected '}'");
        accept(";");
    }
    // if (c) { ... } [else if (c) { ... }] [else { ... }]: every local either branch sets becomes
    // c ? its value there : its value in the other (or before the if).
    void ifStatement() {
        if (!accept("(")) fail("expected '(' after if");
        Local condition;
        std::vector<Op>* outer = out_;
        out_ = &condition.ops;
        const Ty type = expression();
        out_ = outer;
        if (type == kVector) fail("an if's condition is a number, not a vector");
        if (!accept(")")) fail("expected ')' after the if's condition");
        const std::map<std::string, Local> before = locals_;
        std::set<std::string>* outerAssigned = assigned_;
        std::set<std::string> thenNames, elseNames;
        assigned_ = &thenNames;
        block();
        std::map<std::string, Local> thenLocals = std::move(locals_);
        locals_ = before;
        assigned_ = &elseNames;
        const std::size_t save = pos_;
        std::string word;
        if (readName(word) && word == "else") {
            const std::size_t afterElse = pos_;
            std::string next;
            if (readName(next) && next == "if" && peek('(')) {
                ifStatement();
            } else {
                pos_ = afterElse;
                block();
            }
        } else {
            pos_ = save;
        }
        std::map<std::string, Local> elseLocals = std::move(locals_);
        locals_ = before;
        assigned_ = outerAssigned;
        std::set<std::string> names = thenNames;
        names.insert(elseNames.begin(), elseNames.end());
        for (const std::string& name : names) {
            locals_[name] = merged(condition, name, thenLocals, elseLocals);
            if (assigned_) assigned_->insert(name);
        }
    }
    // A name's value in a branch: its local there, or the variable it names when not one.
    Local branchValue(const std::string& name, const std::map<std::string, Local>& locals, bool* found) {
        *found = true;
        if (const auto it = locals.find(name); it != locals.end()) return it->second;
        Local value;
        if (!knownName(name)) {
            *found = false;
            return value;
        }
        std::vector<Op>* outer = out_;
        out_ = &value.ops;
        value.type = named(name);
        out_ = outer;
        return value;
    }
    Local merged(const Local& condition, const std::string& name, const std::map<std::string, Local>& thenLocals,
                 const std::map<std::string, Local>& elseLocals) {
        bool inThen = false, inElse = false;
        const Local yes = branchValue(name, thenLocals, &inThen);
        const Local no = branchValue(name, elseLocals, &inElse);
        Local out;
        if (!inThen || !inElse) {
            out.error = "$" + name + " is set in only one branch of an if above: set it before the if too, or in both branches";
            return out;
        }
        if (!yes.error.empty()) return yes;
        if (!no.error.empty()) return no;
        if (condition.ops.size() + yes.ops.size() + no.ops.size() + 3 > kMaxProgram) {
            fail("the expression is too long once $" + name + "'s if is put in where it is read");
        }
        out.ops = condition.ops;
        out.ops.insert(out.ops.end(), yes.ops.begin(), yes.ops.end());
        if (yes.type == kNumber && no.type == kVector) out.ops.push_back(makeOp(Op::Splat));
        out.ops.insert(out.ops.end(), no.ops.begin(), no.ops.end());
        if (no.type == kNumber && yes.type == kVector) out.ops.push_back(makeOp(Op::Splat));
        const bool vector = yes.type == kVector || no.type == kVector;
        out.ops.push_back(makeOp(vector ? Op::VSelect : Op::Select));
        out.type = vector ? kVector : kNumber;
        return out;
    }

    // An expression: x -> f(a) -> g(b) applies f, then g (the lowest precedence, left to right:
    // everything before the -> is f's first argument). Operators after a call take its value as
    // their first operand, as SeExpr's grammar does: x->f() * 2 is f(x) * 2.
    Ty expression() {
        const Deeper deeper(*this);
        const std::size_t start = out_->size();
        Ty type = conditional();
        while (accept("->")) {
            std::string name;
            if (!readName(name) || name.find('.') != std::string::npos) fail("expected a function's name after '->'");
            if (!accept("(")) fail("expected '(' after " + name + " (-> applies a function: x->clamp(0, 1) is clamp(x, 0, 1))");
            const Applied applied{start, type};
            pendingType_ = call(name, &applied);
            pending_ = true;
            type = conditional();  // starts from the call's value (primary() gives it)
        }
        return type;
    }
    Ty conditional() {
        const Deeper deeper(*this);
        const Ty condition = logicalOr();
        if (!accept("?")) return condition;
        if (condition == kVector) fail("a condition is a number, not a vector");
        const Ty yes = expression();
        const std::size_t yesEnd = out_->size();
        if (!accept(":")) fail("expected ':'");
        const Ty no = conditional();
        if (yes == kNumber && no == kNumber) {
            emit(makeOp(Op::Select));
            return kNumber;
        }
        // A number beside a vector: on every component.
        if (yes == kNumber) insertAt(yesEnd, makeOp(Op::Splat));
        if (no == kNumber) emit(makeOp(Op::Splat));
        emit(makeOp(Op::VSelect));
        return kVector;
    }
    // a || b: 1 when either is not 0; a && b: 1 when both are not. Both sides are evaluated (an
    // expression has no side effects), as comparisons the GPU runs too.
    void truth(Ty type, const char* op) {
        if (type == kVector) fail(std::string(op) + " takes numbers, not vectors");
        emit(makeOp(Op::Number, 0, 0.0f));
        emit(makeOp(Op::Binary, kNe));
    }
    Ty logicalOr() {
        Ty type = logicalAnd();
        while (accept("||")) {
            truth(type, "||");
            truth(logicalAnd(), "||");
            emit(makeOp(Op::Binary, kAdd));
            emit(makeOp(Op::Number, 0, 0.0f));
            emit(makeOp(Op::Binary, kGt));
            type = kNumber;
        }
        return type;
    }
    Ty logicalAnd() {
        Ty type = equality();
        while (accept("&&")) {
            truth(type, "&&");
            truth(equality(), "&&");
            emit(makeOp(Op::Binary, kMul));
            type = kNumber;
        }
        return type;
    }
    void comparisonOf(Bin b, Ty left, Ty right) {
        if (left == kVector || right == kVector) fail("comparisons are of numbers, not vectors (compare .x, length(...) or dot(...))");
        emit(makeOp(Op::Binary, b));
    }
    Ty equality() {
        Ty type = relational();
        for (;;) {
            if (accept("==")) comparisonOf(kEq, type, relational());
            else if (accept("!=")) comparisonOf(kNe, type, relational());
            else return type;
            type = kNumber;
        }
    }
    Ty relational() {
        Ty type = additive();
        for (;;) {
            if (accept("<=")) comparisonOf(kLe, type, additive());
            else if (accept(">=")) comparisonOf(kGe, type, additive());
            else if (accept("<")) comparisonOf(kLt, type, additive());
            else if (accept(">")) comparisonOf(kGt, type, additive());
            else return type;
            type = kNumber;
        }
    }
    Ty additive() {
        Ty type = multiplicative();
        for (;;) {
            if (accept("+")) type = arithmetic(kAdd, type, multiplicative());
            else if (acceptMinus()) type = arithmetic(kSub, type, multiplicative());
            else return type;
        }
    }
    Ty multiplicative() {
        Ty type = unary();
        for (;;) {
            if (accept("*")) type = arithmetic(kMul, type, unary());
            else if (accept("/")) type = arithmetic(kDiv, type, unary());
            else if (accept("%")) type = arithmetic(kFmodOp, type, unary());
            else return type;
        }
    }
    Ty unary() {
        const Deeper deeper(*this);
        if (pending_) return power();  // an apply's value: a - after it is a subtraction
        if (acceptMinus()) {
            const Ty type = unary();
            emit(makeOp(type == kVector ? Op::VUnary : Op::Unary));
            return type;
        }
        if (accept("+")) return unary();
        if (!peekNotEqual() && accept("!")) {  // not: 1 for 0, else 0
            if (unary() == kVector) fail("! takes a number, not a vector");
            emit(makeOp(Op::Number, 0, 0.0f));
            emit(makeOp(Op::Binary, kEq));
            return kNumber;
        }
        if (accept("~")) {  // invert: 1 - x
            const Ty type = unary();
            emit(makeOp(type == kVector ? Op::VUnary : Op::Unary));
            emit(makeOp(Op::Number, 0, 1.0f));
            return arithmetic(kAdd, type, kNumber);
        }
        return power();
    }
    bool peekNotEqual() {
        skipSpace();
        return s_.compare(pos_, 2, "!=") == 0;
    }
    Ty power() {
        const Ty base = postfix(primary());
        if (accept("^")) {
            const Ty exponent = unary();  // right-associative, and 2^-1 works
            return arithmetic(kPowOp, base, exponent);
        }
        return base;
    }
    // A vector's component right after it: v[0], v[$i], (a + b).x.
    Ty postfix(Ty type) {
        for (;;) {
            if (pos_ >= s_.size()) return type;
            if (s_[pos_] == '[') {
                if (type != kVector) fail("a number has no components to index");
                ++pos_;
                const std::size_t start = out_->size();
                if (expression() != kNumber) fail("a vector's index is a number");
                if (!accept("]")) fail("expected ']' after the index");
                float index = 0.0f;
                if (fold(start, out_->size(), &index)) {
                    if (index != 0.0f && index != 1.0f && index != 2.0f) fail("a vector's index is 0, 1 or 2");
                    out_->erase(out_->begin() + static_cast<std::ptrdiff_t>(start), out_->end());
                    emit(makeOp(Op::Component, static_cast<std::uint8_t>(index)));
                } else {  // an index read per strand: 0, 1, 2, else 0
                    Op op = makeOp(Op::Call, kIndexFn);
                    op.args = 4;
                    emit(op);
                }
                type = kNumber;
                continue;
            }
            if (s_[pos_] == '.' && pos_ + 1 < s_.size() && std::strchr("xyzrgb", s_[pos_ + 1]) &&
                (pos_ + 2 >= s_.size() || !(std::isalnum(static_cast<unsigned char>(s_[pos_ + 2])) || s_[pos_ + 2] == '_'))) {
                if (type != kVector) fail("a number has no ." + std::string(1, s_[pos_ + 1]));
                emit(makeOp(Op::Component, static_cast<std::uint8_t>((std::strchr("xyzrgb", s_[pos_ + 1]) - "xyzrgb") % 3)));
                pos_ += 2;
                type = kNumber;
                continue;
            }
            return type;
        }
    }
    Ty primary() {
        if (pending_) {  // an apply's value, compiled already
            pending_ = false;
            return pendingType_;
        }
        skipSpace();
        if (pos_ >= s_.size()) fail("unexpected end");
        const char c = s_[pos_];
        if (accept("(")) {
            const Ty type = expression();
            if (!accept(")")) fail("expected ')'");
            return type;
        }
        if (accept("[")) {  // a vector [x, y, z]
            for (int k = 0; k < 3; ++k) {
                if (expression() != kNumber) fail("a vector's components are numbers");
                if (k < 2 && !accept(",")) fail("a vector is [x, y, z]: expected ','");
            }
            if (!accept("]")) fail("a vector is [x, y, z]: expected ']'");
            return kVector;
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            const char* begin = s_.c_str() + pos_;
            char* end = nullptr;
            const double value = std::strtod(begin, &end);
            if (end == begin) fail("bad number");
            pos_ += static_cast<std::size_t>(end - begin);
            emit(makeOp(Op::Number, 0, static_cast<float>(value)));
            return kNumber;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            std::string name;
            if (!readName(name)) fail("expected a name after '$'");
            if (accept("(")) return call(name, nullptr);
            return named(name);
        }
        if (c == '"' || c == '\'') fail("text in quotes names a map, in map(\"name\")");
        fail("unexpected '" + std::string(1, c) + "'");
    }
    void insertLocal(const std::string& name, const Local& local) {
        if (!local.error.empty()) fail(local.error);
        if (out_->size() + local.ops.size() > kMaxProgram) {
            fail("the expression is too long once $" + name + " is put in where it is read (" +
                 std::to_string(kMaxProgram) + " operations at most): read it in fewer places");
        }
        out_->insert(out_->end(), local.ops.begin(), local.ops.end());
    }
    const float* constant(const std::string& name) const {
        for (const auto& [n, value] : constants_) {
            if (n == name) return &value;
        }
        return nullptr;
    }
    static std::string variableName(const std::string& name) {
        const char* to = aliasOf(kVariableAliases, name);
        return to ? std::string(to) : name;
    }
    void unsupportedVariable(const std::string& name) const {
        if (const char* why = aliasOf(kUnsupportedVariables, name)) fail(why);
    }
    // A name named() reads without making it a channel: the node's own variables, the built-in ones.
    bool knownName(const std::string& name) const {
        if (constant(name) || constant(name + ".x")) return true;
        const std::string base = variableName(name);
        if (base == "F" || base == "pi") return true;
        for (const VectorVar& v : kVectorVars) {
            if (base == v.name) return true;
        }
        for (const char* v : kVarNames) {
            if (base == v) return true;
        }
        return false;
    }
    // A name: a local computed above, then the node's own variables, then the built-in ones,
    // then a channel.
    Ty named(const std::string& name) {
        if (const auto local = locals_.find(name); local != locals_.end()) {
            insertLocal(name, local->second);
            return local->second.type;
        }
        const std::size_t dot = name.find('.');
        if (dot != std::string::npos) {
            const std::string base = name.substr(0, dot);
            const auto k = static_cast<std::uint8_t>((std::strchr("xyzrgb", name[dot + 1]) - "xyzrgb") % 3);
            if (const auto local = locals_.find(base); local != locals_.end()) {
                if (local->second.type != kVector) fail("$" + base + " is a number: it has no ." + name.substr(dot + 1));
                insertLocal(base, local->second);
                emit(makeOp(Op::Component, k));
                return kNumber;
            }
            if (const float* value = constant(name)) {
                emit(makeOp(Op::Number, 0, *value));
                return kNumber;
            }
            const std::string vector = variableName(base);
            unsupportedVariable(vector);
            for (const VectorVar& v : kVectorVars) {
                if (vector == v.name) {
                    emit(makeOp(Op::Variable, k == 0 ? v.x : (k == 1 ? v.y : v.z)));
                    return kNumber;
                }
            }
            fail("unknown vector $" + base);
        }
        if (const float* value = constant(name)) {
            emit(makeOp(Op::Number, 0, *value));
            return kNumber;
        }
        if (const float* x = constant(name + ".x")) {  // the node's own vector, whole
            const float* y = constant(name + ".y");
            const float* z = constant(name + ".z");
            emit(makeOp(Op::Number, 0, *x));
            emit(makeOp(Op::Number, 0, y ? *y : 0.0f));
            emit(makeOp(Op::Number, 0, z ? *z : 0.0f));
            return kVector;
        }
        const std::string variable = variableName(name);
        unsupportedVariable(variable);
        for (const VectorVar& v : kVectorVars) {
            if (variable == v.name) {
                emit(makeOp(Op::Variable, v.x));
                emit(makeOp(Op::Variable, v.y));
                emit(makeOp(Op::Variable, v.z));
                return kVector;
            }
        }
        if (variable == "F") {
            emit(makeOp(Op::Variable, kFrame));
            return kNumber;
        }
        for (std::uint8_t v = 0; v < kVarCount; ++v) {
            if (variable == kVarNames[v]) {
                emit(makeOp(Op::Variable, v));
                return kNumber;
            }
        }
        if (variable == "pi") {
            emit(makeOp(Op::Number, 0, 3.14159265f));
            return kNumber;
        }
        auto it = std::find(channels_.begin(), channels_.end(), name);
        if (it == channels_.end()) {
            channels_.push_back(name);
            it = channels_.end() - 1;
        }
        Op op = makeOp(Op::Channel);
        op.channel = static_cast<std::uint16_t>(it - channels_.begin());
        emit(op);
        return kNumber;
    }

    // A string in quotes (a map's name, a channel's).
    std::string quoted() {
        skipSpace();
        if (pos_ >= s_.size() || (s_[pos_] != '"' && s_[pos_] != '\'')) fail("expected a name in quotes");
        const char quote = s_[pos_++];
        const std::size_t end = s_.find(quote, pos_);
        if (end == std::string::npos) fail("the quotes are not closed");
        std::string text = s_.substr(pos_, end - pos_);
        pos_ = end + 1;
        return text;
    }

    Ty mapCall(Fn fn, const std::string& name) {
        const std::string written = quoted();
        const std::string mapName = expressionMapName(written);
        if (mapName.empty() || mapName.find_first_of("${}%") != std::string::npos) {
            fail(name + "('" + written + "'): give the name of one of the groom's maps, map(\"name\")");
        }
        int component = 4;  // luminance
        if (accept(",")) {
            if (fn == kCMap) fail(name + " takes the map's name alone (it gives r, g, b)");
            skipSpace();
            if (pos_ < s_.size() && (s_[pos_] == '"' || s_[pos_] == '\'')) {
                const std::string channel = quoted();
                const char* names[] = {"r", "g", "b", "a", "luminance"};
                component = -1;
                for (int k = 0; k < 5; ++k) {
                    if (channel == names[k]) component = k;
                }
                if (channel == "l" || channel == "lum") component = 4;
                if (component < 0) fail("a map's channel is \"r\", \"g\", \"b\", \"a\" or \"luminance\"");
            } else {
                const char* begin = s_.c_str() + pos_;
                char* end = nullptr;
                const double value = std::strtod(begin, &end);
                if (end == begin || value < 0.0 || value > 4.0 || value != std::floor(value)) {
                    fail("a map's channel is 0 (r), 1 (g), 2 (b), 3 (a) or 4 (luminance)");
                }
                pos_ += static_cast<std::size_t>(end - begin);
                component = static_cast<int>(value);
            }
        }
        if (!accept(")")) fail("expected ')' after the arguments of " + name);
        auto it = std::find(mapNames_.begin(), mapNames_.end(), mapName);
        if (it == mapNames_.end()) {
            ExpressionMapPtr map;
            if (environment_ && environment_->map) {
                map = environment_->map(mapName);
                if (!map && written != mapName) {
                    fail(name + "('" + written + "'): XGen's paint maps do not carry over; GroomLab reads the groom's own maps by name, here '" +
                         mapName + "', and the groom has none of that name (paint one in Groom Tools > Paint, or write map(\"name\"))");
                }
                if (!map) fail("no map named '" + mapName + "' (Groom Tools > Paint lists them)");
            }
            mapNames_.push_back(mapName);
            maps_.push_back(std::move(map));
            it = mapNames_.end() - 1;
        }
        Op op = makeOp(Op::Call, fn, static_cast<float>(component));
        op.args = 0;
        op.results = kFns[fn].results == 3 ? 3 : 1;
        op.channel = static_cast<std::uint16_t>(it - mapNames_.begin());
        emit(op);
        return op.results == 3 ? kVector : kNumber;
    }

    // The value of the ops [begin, end) when they read nothing of a strand (numbers, the node's
    // own variables, math on them), `slots` of them (1: a number, 3: a vector); false otherwise.
    bool fold(std::size_t begin, std::size_t end, float* value, std::size_t slots = 1) const {
        for (std::size_t i = begin; i < end; ++i) {
            const Op& op = (*out_)[i];
            if (op.kind == Op::Variable || op.kind == Op::Channel) return false;
            if ((op.kind == Op::Call || op.kind == Op::VCall) &&
                (op.code == kRandFn || op.code == kRandRange || op.code == kMap || op.code == kCMap || op.code >= kCurve)) {
                return false;
            }
        }
        std::size_t depth = 0, deepest = 0;
        for (std::size_t i = begin; i < end; ++i) deepest = std::max(deepest, depth = depthAfter((*out_)[i], depth));
        if (deepest > kMaxStack || depth != slots) return false;
        float last[3] = {0.0f, 0.0f, 0.0f};
        const EnvData env;
        const Tables none;
        const std::size_t top = slots == 3 ? execOps<true>(out_->data() + begin, out_->data() + end, env, none, last)
                                           : execOps<false>(out_->data() + begin, out_->data() + end, env, none, last);
        if (top != slots) return false;
        for (std::size_t k = 0; k < slots; ++k) value[k] = finite(last[k]);
        return true;
    }

    // An argument's range of ops, in the program.
    struct Args {
        std::vector<std::size_t> starts, ends;
        std::vector<Ty> types;
        std::size_t size() const { return types.size(); }
    };

    // Takes the keys' ops [starts[1], end of the program) off the program, folded to numbers
    // (`width` per key: a vector value three); fails naming `name` when one reads the strand.
    std::vector<float> foldKeys(const std::string& name, const Args& args, const std::vector<int>& widths) {
        std::vector<float> keys;
        const std::size_t count = args.size();
        std::vector<std::vector<float>> each(count);
        for (std::size_t i = count; i-- > 1;) {
            const std::size_t end = i + 1 < count ? args.starts[i + 1] : out_->size();
            const int width = widths[(i - 1) % widths.size()];
            each[i].resize(static_cast<std::size_t>(width));
            const bool ok = width == 3 && args.types[i] == kNumber
                                ? fold(args.starts[i], end, each[i].data()) && ((each[i][1] = each[i][2] = each[i][0]), true)
                                : fold(args.starts[i], end, each[i].data(), static_cast<std::size_t>(width));
            if (!ok) fail(name + "'s keys are numbers (or the node's own variables), fixed when the expression compiles");
            out_->erase(out_->begin() + static_cast<std::ptrdiff_t>(args.starts[i]), out_->begin() + static_cast<std::ptrdiff_t>(end));
        }
        for (std::size_t i = 1; i < count; ++i) keys.insert(keys.end(), each[i].begin(), each[i].end());
        return keys;
    }

    // curve(t, p0, v0, i0, ...) can be SeExpr's: a multiple of three keys, every interpolation a
    // constant 0 to 4.
    bool seexprCurve(const Args& args) const {
        const std::size_t keys = args.size() - 1;
        if (keys == 0 || keys % 3 != 0) return false;
        for (std::size_t i = 3; i < args.size(); i += 3) {
            float interp = 0.0f;
            if (args.types[i] != kNumber || !fold(args.starts[i], args.ends[i], &interp)) return false;
            if (interp != std::floor(interp) || interp < 0.0f || interp > 4.0f) return false;
        }
        return true;
    }

    Ty curveCall(Fn fn, const std::string& name, const Args& args) {
        const std::size_t count = args.size();
        if (args.types[0] != kNumber) fail(name + "'s first argument (where on the curve) is a number");
        if (fn == kCurve || fn == kCCurve) {
            if ((count - 1) % 3 != 0 || count < 4) {
                fail(name + " takes (t, position, value, interpolation, ...): keys of three, the interpolation 0 (none), 1 "
                            "(linear), 2 (smooth), 3 (spline) or 4 (monotone spline)");
            }
            for (std::size_t i = 1; i < count; ++i) {
                if (args.types[i] == kVector && (fn == kCurve || (i - 1) % 3 != 1)) fail(name + "'s keys are numbers" + (fn == kCCurve ? std::string(" (a value may be a color)") : std::string()));
            }
            const std::vector<float> keys = foldKeys(name, args, fn == kCCurve ? std::vector<int>{1, 3, 1} : std::vector<int>{1});
            ExpressionCurve curve;
            const std::size_t stride = fn == kCCurve ? 5 : 3;
            for (std::size_t i = 0; i + stride <= keys.size(); i += stride) {
                ExpressionCurve::Key key;
                key.position = keys[i];
                key.value = fn == kCCurve ? Vec3f{keys[i + 1], keys[i + 2], keys[i + 3]} : Vec3f{keys[i + 1], keys[i + 1], keys[i + 1]};
                const float interp = keys[i + stride - 1];
                if (interp != std::floor(interp) || interp < 0.0f || interp > 4.0f) {
                    fail(name + "'s interpolations are 0 (none), 1 (linear), 2 (smooth), 3 (spline) or 4 (monotone spline)");
                }
                key.interpolation = static_cast<int>(interp);
                curve.keys.push_back(key);
            }
            curve.prepare();
            if (curves_.size() >= 65535) fail("too many curves");
            curves_.push_back(std::move(curve));
            Op op = makeOp(Op::Call, fn);
            op.args = 1;
            op.results = fn == kCCurve ? 3 : 1;
            op.channel = static_cast<std::uint16_t>(curves_.size() - 1);
            emit(op);
            return fn == kCCurve ? kVector : kNumber;
        }
        // GroomLab's pairs: a smooth ramp.h curve, or straight segments.
        for (const Ty t : args.types) {
            if (t != kNumber) fail(name + "'s arguments are numbers");
        }
        if ((count - 1) % 2 != 0) fail(name + " takes t and pairs of position and value: (t, p0, v0, p1, v1, ...)");
        const std::vector<float> keys = foldKeys(name, args, {1});
        Ramp ramp;
        for (std::size_t i = 0; i + 1 < keys.size(); i += 2) ramp.keys.emplace_back(keys[i], keys[i + 1]);
        ramp.straight = fn == kLinearCurve;
        ramp.sort();
        if (ramps_.size() >= 65535) fail("too many curves");
        ramps_.push_back(std::move(ramp));
        Op op = makeOp(Op::Call, fn);
        op.args = 1;
        op.channel = static_cast<std::uint16_t>(ramps_.size() - 1);
        emit(op);
        return kNumber;
    }

    // smoothstep(x, a, b) and linearstep(x, a, b) are SeExpr's; the program keeps the edges first.
    // GroomLab's older order, (e0, e1, x), when e0 and e1 are constants and x is not.
    void edgesFirst(Args& args) {
        float unused = 0.0f;
        const bool c0 = fold(args.starts[0], args.ends[0], &unused);
        const bool c1 = fold(args.starts[1], args.ends[1], &unused);
        const bool c2 = fold(args.starts[2], args.ends[2], &unused);
        if (c0 && c1 && !c2) return;  // edges first already
        // x, a, b -> a, b, x
        const std::size_t s0 = args.starts[0], e0 = args.ends[0], e2 = args.ends[2];
        const std::size_t lx = e0 - s0, la = args.ends[1] - args.starts[1], lb = e2 - args.starts[2];
        std::rotate(out_->begin() + static_cast<std::ptrdiff_t>(s0), out_->begin() + static_cast<std::ptrdiff_t>(e0),
                    out_->begin() + static_cast<std::ptrdiff_t>(e2));
        args.starts = {s0, s0 + la, s0 + la + lb};
        args.ends = {s0 + la, s0 + la + lb, s0 + la + lb + lx};
        args.types = {args.types[1], args.types[2], args.types[0]};
    }

    Ty call(const std::string& written, const Applied* applied) {
        const std::string name = canonicalFunction(written);
        if (const char* why = aliasOf(kUnsupportedFunctions, name)) fail(why);
        Fn fn = kFnCount;
        for (std::uint8_t f = 0; f < kFnCount; ++f) {
            if (name == kFns[f].name) {
                fn = static_cast<Fn>(f);
                break;
            }
        }
        if (fn == kFnCount) fail("unknown function '" + written + "'");
        if (kFns[fn].kind == FnKind::Map) {
            if (applied) fail(written + " takes a map's name in quotes; -> can't give it one");
            return mapCall(fn, written);
        }
        Args args;
        if (applied) {
            args.starts.push_back(applied->start);
            args.types.push_back(applied->type);
            args.ends.push_back(out_->size());
        }
        if (!accept(")")) {
            do {
                args.starts.push_back(out_->size());
                args.types.push_back(expression());
                args.ends.push_back(out_->size());
            } while (accept(","));
            if (!accept(")")) fail("expected ')' after the arguments of " + written);
        }
        auto count = static_cast<int>(args.size());
        const bool anyVector = std::find(args.types.begin(), args.types.end(), kVector) != args.types.end();
        // Forms one name has.
        if (fn == kNoise) {  // noise(p), noise(x[, y[, z[, w]]])
            if (count == 1) {
                fn = kNoiseP;
            } else if (count >= 2 && count <= 4) {
                if (anyVector) fail("noise takes a point, noise(p), or 2 to 4 numbers, noise(x, y, z, w)");
                if (count == 2) {
                    emit(makeOp(Op::Number, 0, 0.0f));
                    count = 3;
                }
                Op op = makeOp(Op::Call, count == 4 ? kNoise4 : kNoise);
                op.args = static_cast<std::uint8_t>(count);
                emit(op);
                return kNumber;
            } else {
                fail("noise takes a point, noise(p), or 2 to 4 numbers, noise(x, y, z, w)");
            }
        }
        if (fn == kRandFn) {  // rand() per strand, rand(seed), rand(min, max[, seed])
            if (count == 0) {
                emit(makeOp(Op::Variable, kRand));
                return kNumber;
            }
            if (count == 2 || count == 3) fn = kRandRange;
        }
        if (fn == kCurve && !seexprCurve(args)) {
            if ((count - 1) % 2 == 0 && count >= 3) {
                fn = kRampCurve;  // GroomLab's pairs
            } else {
                fail("curve takes (t, position, value, interpolation, ...) as SeExpr: keys of three, the interpolation 0 "
                     "(none), 1 (linear), 2 (smooth), 3 (spline) or 4 (monotone spline)");
            }
        }
        if (fn == kWChoose && count % 2 == 0) fail("wchoose takes (t, value, weight, value, weight, ...)");
        const FnInfo& used = kFns[fn];
        if (count < used.minArgs || count > used.maxArgs) {
            const std::string takes = used.minArgs == used.maxArgs ? std::to_string(used.minArgs)
                                                                   : std::to_string(used.minArgs) + " to " + std::to_string(used.maxArgs);
            fail(written + " takes " + takes + " argument(s), got " + std::to_string(count));
        }
        if ((fn == kSmoothstep || fn == kLinearstep) && count == 3) edgesFirst(args);
        Op op = makeOp(Op::Call, fn);
        switch (used.kind) {
            case FnKind::Math:
                if (!anyVector) {
                    op.args = static_cast<std::uint8_t>(count);
                    emit(op);
                    return kNumber;
                }
                // Component by component: every number argument made a vector (last first, so
                // the earlier places stay where they are).
                if (3 * count > static_cast<int>(kMaxStack)) fail(written + " takes too many vectors");
                for (int i = count; i-- > 0;) {
                    if (args.types[static_cast<std::size_t>(i)] == kNumber) insertAt(args.ends[static_cast<std::size_t>(i)], makeOp(Op::Splat));
                }
                op.kind = Op::VCall;
                op.args = static_cast<std::uint8_t>(3 * count);
                op.results = 3;
                emit(op);
                return kVector;
            case FnKind::Scalar:
                if (anyVector) fail(written + "'s arguments are numbers");
                op.args = static_cast<std::uint8_t>(count);
                emit(op);
                return kNumber;
            case FnKind::Vector: {
                const char* signature = used.signature;
                for (int i = count; i-- > 0;) {
                    const auto at = static_cast<std::size_t>(i);
                    const char want = signature[i];
                    if (want == 's' && args.types[at] == kVector) {
                        fail("argument " + std::to_string(i + 1) + " of " + written + " is a number, not a vector");
                    }
                    if (args.types[at] == kNumber && want == 'v') insertAt(args.ends[at], makeOp(Op::Splat));
                    if (args.types[at] == kNumber && want == 'p') {  // a point x: [x, 0, 0]
                        insertAt(args.ends[at], makeOp(Op::Number, 0, 0.0f));
                        insertAt(args.ends[at], makeOp(Op::Number, 0, 0.0f));
                    }
                }
                std::size_t slots = 0;
                for (int i = 0; i < used.maxArgs; ++i) {
                    const bool number = signature[i] == 's';
                    if (i >= count) {  // at its default
                        const float value = used.defaults[static_cast<std::size_t>(i)];
                        for (int k = 0; k < (number ? 1 : 3); ++k) emit(makeOp(Op::Number, 0, value));
                    }
                    slots += number ? 1 : 3;
                }
                op.args = static_cast<std::uint8_t>(slots);
                op.results = static_cast<std::uint8_t>(used.results);
                emit(op);
                return used.results == 3 ? kVector : kNumber;
            }
            case FnKind::Curve: return curveCall(fn, written, args);
            case FnKind::Map: break;
        }
        fail("unknown function '" + written + "'");
    }

    const std::string& s_;
    std::size_t pos_ = 0;
    int depth_ = 0;
    std::vector<Op>* out_;
    std::vector<std::string>& channels_;
    const std::vector<std::pair<std::string, float>>& constants_;
    std::vector<Ramp>& ramps_;
    std::vector<ExpressionCurve>& curves_;
    std::vector<std::string>& mapNames_;
    std::vector<ExpressionMapPtr>& maps_;
    const ExpressionEnvironment* environment_;
    std::map<std::string, Local> locals_;
    std::set<std::string>* assigned_ = nullptr;  // the names an if's branch sets
    bool pending_ = false;  // primary() gives an apply's value (of type pendingType_) next
    Ty pendingType_ = kNumber;
};

thread_local const ExpressionEnvironment* currentEnvironment = nullptr;

}  // namespace

ExpressionEnvironmentScope::ExpressionEnvironmentScope(const ExpressionEnvironment* environment)
    : saved_(currentEnvironment) {
    currentEnvironment = environment;
}

ExpressionEnvironmentScope::~ExpressionEnvironmentScope() { currentEnvironment = saved_; }

const ExpressionEnvironment* ExpressionEnvironmentScope::current() { return currentEnvironment; }

std::string expressionMapName(const std::string& text) {
    if (text.find_first_of("/\\") == std::string::npos && text.find("${") == std::string::npos) return text;
    std::string name = text;
    while (!name.empty() && (name.back() == '/' || name.back() == '\\')) name.pop_back();
    const std::size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const std::size_t dot = name.rfind('.');
    if (dot != std::string::npos) {
        std::string extension = name.substr(dot + 1);
        for (char& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (const char* image : {"ptx", "tx", "tif", "tiff", "png", "exr", "jpg", "jpeg", "tga"}) {
            if (extension == image) {
                name = name.substr(0, dot);
                break;
            }
        }
    }
    return name;
}

void ExpressionCurve::prepare() {
    std::stable_sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) { return a.position < b.position; });
    const std::size_t n = keys.size();
    // Catmull-Rom slopes (centred differences); the ends flat (SeExpr's sentinels, far away).
    for (std::size_t i = 0; i < n; ++i) {
        keys[i].slope = Vec3f{};
        if (i == 0 || i + 1 >= n) continue;
        const float span = keys[i + 1].position - keys[i - 1].position;
        if (span > 0.0f) keys[i].slope = (keys[i + 1].value - keys[i - 1].value) * (1.0f / span);
    }
    // A monotone span: slopes kept from overshooting (0 to 3 times the span's own, per component).
    for (std::size_t i = 0; i + 1 < n; ++i) {
        if (keys[i].interpolation != 4) continue;
        const float h = keys[i + 1].position - keys[i].position;
        if (!(h > 0.0f)) continue;
        const Vec3f delta = (keys[i + 1].value - keys[i].value) * (1.0f / h);
        float* d1 = &keys[i].slope.x;
        float* d2 = &keys[i + 1].slope.x;
        const float* dd = &delta.x;
        for (int k = 0; k < 3; ++k) {
            if (dd[k] == 0.0f) {
                d1[k] = d2[k] = 0.0f;
            } else {
                d1[k] = std::clamp(d1[k] / dd[k], 0.0f, 3.0f) * dd[k];
                d2[k] = std::clamp(d2[k] / dd[k], 0.0f, 3.0f) * dd[k];
            }
        }
    }
}

Vec3f ExpressionCurve::operator()(float t) const {
    const std::size_t n = keys.size();
    if (n == 0) return Vec3f{};
    if (!(t >= keys[0].position)) return keys[0].value;  // also NaN
    if (t >= keys[n - 1].position) return keys[n - 1].value;
    // The span [i, i + 1) holding t: the last key at or before it.
    std::size_t i = static_cast<std::size_t>(
        std::upper_bound(keys.begin(), keys.end(), t, [](float v, const Key& k) { return v < k.position; }) - keys.begin()) - 1;
    const Key& a = keys[i];
    const Key& b = keys[i + 1];
    const float h = b.position - a.position;
    if (!(h > 0.0f)) return b.value;
    const float u = (t - a.position) / h;
    switch (a.interpolation) {
        case 0: return a.value;
        case 1: return a.value + (b.value - a.value) * u;
        case 2: return a.value * ((u - 1.0f) * (u - 1.0f) * (2.0f * u + 1.0f)) + b.value * (u * u * (3.0f - 2.0f * u));
        default: {  // 3, 4: Hermite with the keys' slopes
            const float x = t - a.position;
            const Vec3f delta = b.value - a.value;
            const Vec3f d1 = a.slope, d2 = b.slope;
            return (delta * ((3.0f * h - 2.0f * x) * x) + (d1 * -h + (d1 + d2) * x) * (h * (x - h))) * (x / (h * h * h)) + a.value;
        }
    }
}

ExpressionDependencies scanExpression(const std::string& text) {
    ExpressionDependencies out;
    const std::size_t n = text.size();
    auto ident = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
    std::size_t i = 0;
    while (i < n) {
        const char c = text[i];
        if (c == '#' || (c == '/' && i + 1 < n && text[i + 1] == '/')) {
            while (i < n && text[i] != '\n') ++i;
            continue;
        }
        if (c == '"' || c == '\'') {  // a string not after map( (an error): skipped
            const std::size_t end = text.find(c, i + 1);
            i = end == std::string::npos ? n : end + 1;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            while (i < n && (ident(text[i]) || text[i] == '.')) ++i;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            const bool component = i > 0 && text[i - 1] == '.';
            std::size_t start = c == '$' ? i + 1 : i;
            std::size_t end = start;
            while (end < n && ident(text[end])) ++end;
            const std::string name = text.substr(start, end - start);
            std::size_t k = end;
            while (k < n && std::isspace(static_cast<unsigned char>(text[k]))) ++k;
            const bool called = k < n && text[k] == '(';
            if (called && (name == "map" || name == "cmap" || name == "vmap")) {
                std::size_t q = k + 1;
                while (q < n && std::isspace(static_cast<unsigned char>(text[q]))) ++q;
                if (q < n && (text[q] == '"' || text[q] == '\'')) {
                    const std::size_t close = text.find(text[q], q + 1);
                    if (close != std::string::npos) {
                        const std::string map = expressionMapName(text.substr(q + 1, close - q - 1));
                        if (std::find(out.maps.begin(), out.maps.end(), map) == out.maps.end()) out.maps.push_back(map);
                        end = close + 1;
                    }
                }
            } else if (!called && !component && (name == "frame" || name == "F" || name == "time")) {
                out.time = true;
            }
            i = std::max(end, i + 1);
            continue;
        }
        ++i;
    }
    return out;
}

std::vector<std::pair<std::string, float>> parseExpressionVariables(const std::string& json) {
    std::vector<std::pair<std::string, float>> out;
    if (json.empty()) return out;
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    if (j.is_discarded() || !j.is_object()) throw ExpressionError("variables are a JSON object of numbers and vectors");
    for (const auto& [name, value] : j.items()) {
        const bool named = !name.empty() && (std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_') &&
                           std::all_of(name.begin(), name.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; });
        if (!named) throw ExpressionError("'" + name + "' is not a variable name (letters, digits and _)");
        if (value.is_number()) {
            out.emplace_back(name, value.get<float>());
        } else if (value.is_object() && value.contains("value") && value.at("value").is_number()) {
            out.emplace_back(name, value.at("value").get<float>());  // keyable: the control overrides it
        } else if (value.is_array() && value.size() == 3 && value[0].is_number() && value[1].is_number() && value[2].is_number()) {
            const char* axes[] = {"x", "y", "z"};
            const char* channels[] = {"r", "g", "b"};
            for (int k = 0; k < 3; ++k) {
                out.emplace_back(name + "." + axes[k], value[k].get<float>());
                out.emplace_back(name + "." + channels[k], value[k].get<float>());
            }
        } else {
            throw ExpressionError("$" + name + " is neither a number nor a vector of three");
        }
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> expressionVariableControls(const std::string& json) {
    std::vector<std::pair<std::string, std::string>> out;
    const nlohmann::json j = nlohmann::json::parse(json.empty() ? "{}" : json, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return out;
    for (const auto& [name, value] : j.items()) {
        if (value.is_object() && value.contains("control") && value.at("control").is_string() &&
            !value.at("control").get<std::string>().empty()) {
            out.emplace_back(name, value.at("control").get<std::string>());
        }
    }
    return out;
}

struct Expression::Env : EnvData {};

namespace {
float arcOf(const Vec3f* pts, std::size_t n) {
    float total = 0.0f;
    for (std::size_t i = 1; i < n; ++i) total += (pts[i] - pts[i - 1]).length();
    return total;
}
}  // namespace

Expression::Expression(const std::string& text, const std::vector<std::pair<std::string, float>>& constants,
                       ExpressionType want)
    : text_(text) {
    const ExpressionEnvironment* environment = ExpressionEnvironmentScope::current();
    if (environment) {
        seconds_ = environment->seconds;
        frame_ = environment->frame;
    }
    Parser parser(text, program_, channels_, constants, ramps_, curves_, mapNames_, maps_, environment);
    vector_ = parser.parse() == kVector;
    if (want == ExpressionType::Number && vector_) {
        throw ExpressionError("expression '" + text + "' gives a vector; this takes a number (use .x, length(...) or dot(...))");
    }
    if (want == ExpressionType::Vector && !vector_) {
        throw ExpressionError("expression '" + text + "' gives a number; this takes a vector, e.g. [0, -1, 0] or normalize(N)");
    }
    for (const Op& op : program_) {
        if (op.kind != Op::Variable) continue;
        readsRoot_ = readsRoot_ || op.code <= kNZ || op.code == kLift;
        readsRef_ = readsRef_ || (op.code >= kPrefX && op.code <= kPrefZ);
        readsShape_ = readsShape_ || (op.code >= kLength && op.code <= kTipZ);
        readsLongest_ = readsLongest_ || op.code == kLongest;
        readsPoint_ = readsPoint_ || (op.code >= kT && op.code <= kPointIndex);
        readsPointPosition_ = readsPointPosition_ || op.code == kT || (op.code >= kPX && op.code <= kPZ);
        readsTime_ = readsTime_ || op.code == kFrame || op.code == kTime;
    }
    // Stack depth, for evaluation without allocation.
    std::size_t depth = 0;
    for (const Op& op : program_) {
        depth = depthAfter(op, depth);
        stackDepth_ = std::max(stackDepth_, depth);
    }
    if (stackDepth_ > kMaxStack) throw ExpressionError("expression '" + text + "' is nested too deeply");
}

float Expression::run(const Env& env) const {
    const Op* begin = program_.data();
    float last[3] = {0.0f, 0.0f, 0.0f};
    const Tables tables{ramps_.data(), curves_.data(), maps_.data()};
    if (vector_) {  // a vector read as a number (ExpressionType::Any): its x
        execOps<true>(begin, begin + program_.size(), env, tables, last);
        return finite(last[0]);
    }
    execOps<false>(begin, begin + program_.size(), env, tables, last);
    return std::isfinite(last[0]) ? last[0] : 0.0f;
}

Vec3f Expression::runVector(const Env& env) const {
    const Op* begin = program_.data();
    float last[3] = {0.0f, 0.0f, 0.0f};
    const Tables tables{ramps_.data(), curves_.data(), maps_.data()};
    if (!vector_) {
        execOps<false>(begin, begin + program_.size(), env, tables, last);
        const float v = finite(last[0]);
        return {v, v, v};
    }
    execOps<true>(begin, begin + program_.size(), env, tables, last);
    return {finite(last[0]), finite(last[1]), finite(last[2])};
}

namespace {

void rootVariables(std::array<float, kVarCount>& vars, const StrandRoot& root, const ScalpMesh* scalp,
                   const Vec3f& fallbackPosition, bool reference) {
    const bool rooted = scalp && root.face >= 0 && static_cast<std::size_t>(root.face) < scalp->triangleCount();
    const Vec3f p = rooted ? scalp->position(root) : fallbackPosition;
    const Vec3f n = rooted ? scalp->normal(root) : Vec3f{0, 1, 0};
    const ScalpMesh::UV uv = rooted ? scalp->uvAt(root) : ScalpMesh::UV{0.0f, 0.0f};
    vars[kU] = uv[0];
    vars[kV] = uv[1];
    vars[kX] = p.x;
    vars[kY] = p.y;
    vars[kZ] = p.z;
    vars[kNX] = n.x;
    vars[kNY] = n.y;
    vars[kNZ] = n.z;
    if (reference) {
        const Vec3f r = rooted ? scalp->restPositionOr(root, p) : p;
        vars[kPrefX] = r.x;
        vars[kPrefY] = r.y;
        vars[kPrefZ] = r.z;
    }
}

}  // namespace

Expression::Bound Expression::bind(const StrandSet& strands) const {
    Bound bound;
    bound.channels.reserve(channels_.size());
    bound.pointChannels.reserve(channels_.size());
    bound.perPoint = readsPoint_;
    for (const std::string& name : channels_) {
        const std::vector<float>* strand = strands.strandChannel(name);
        const std::vector<float>* point = strand ? nullptr : strands.pointChannel(name);
        bound.channels.push_back(strand);
        bound.pointChannels.push_back(point);
        bound.perPoint = bound.perPoint || point != nullptr;
    }
    static std::atomic<std::uint64_t> binds{0};
    bound.serial = ++binds;
    if (readsLongest_) {  // of the core strands (preview_core.h): the same at every preview density
        const PreviewCore core(strands);
        for (std::size_t s = 0; s < strands.strandCount(); ++s) {
            if (core.core(s)) bound.longest = std::max(bound.longest, arcOf(strands.strandPoints(s), strands.strandPointCount(s)));
        }
    }
    return bound;
}

// Everything about strand s that every point shares: root, random, index,
// channels and the shape variables. Point variables are left at the root's.
void Expression::strandVariables(Env& env, const StrandSet& strands, std::size_t s, const ScalpMesh* scalp,
                                 const Bound& bound) const {
    const std::size_t n = strands.strandPointCount(s);
    const Vec3f* pts = strands.strandPoints(s);
    const Vec3f first = n ? pts[0] : Vec3f{};
    const bool hasRoot = s < strands.roots().size();
    // The root's position, normal and UV only when read (they cost a scalp lookup each).
    if (readsRoot_ || readsRef_) rootVariables(env.vars, hasRoot ? strands.roots()[s] : StrandRoot{}, scalp, first, readsRef_);
    if (!maps_.empty()) env.root = hasRoot ? &strands.roots()[s] : nullptr;  // where map() reads
    env.vars[kRand] = unitFloat(hashCombine(strands.ids()[s], 0xE7A1));
    env.vars[kIndex] = strands.strandCount() > 1 ? float(s) / float(strands.strandCount() - 1) : 0.0f;
    env.vars[kId] = static_cast<float>(strands.ids()[s] & 0xFFFFFFu);
    env.vars[kFaceId] = hasRoot ? static_cast<float>(strands.roots()[s].face) : -1.0f;
    if (readsTime_) {
        env.vars[kFrame] = static_cast<float>(frame_);
        env.vars[kTime] = static_cast<float>(seconds_);
    }
    env.channels = &bound.channels;
    env.pointChannels = &bound.pointChannels;
    env.strands = &strands;
    env.strand = s;
    env.point = strands.strandBegin(s);
    if (readsShape_ && n > 0) {
        const float length = arcOf(pts, n);
        const Vec3f span = pts[n - 1] - pts[0];
        const float reach = span.length();
        const Vec3f normal{env.vars[kNX], env.vars[kNY], env.vars[kNZ]};
        env.vars[kLength] = length;
        env.vars[kReach] = reach;
        env.vars[kStraightness] = length > 1e-8f ? reach / length : 1.0f;
        env.vars[kLift] = reach > 1e-8f && normal.length() > 0.0f ? span.dot(normal.normalized()) / reach : 0.0f;
        env.vars[kTipX] = pts[n - 1].x;
        env.vars[kTipY] = pts[n - 1].y;
        env.vars[kTipZ] = pts[n - 1].z;
    }
    if (readsShape_) env.vars[kLongest] = bound.longest;
    if (readsPoint_ && n > 0) {
        env.vars[kPX] = first.x;
        env.vars[kPY] = first.y;
        env.vars[kPZ] = first.z;
    }
}

static_assert(Expression::kVariables == kVarCount, "expression.h says how many variables there are");

int Expression::variableCode(const std::string& name) {
    for (std::uint8_t v = 0; v < kVarCount; ++v) {
        if (name == kVarNames[v]) return v;
    }
    return -1;
}

int Expression::functionCode(const std::string& name) {
    const std::string canonical = canonicalFunction(name);
    for (std::uint8_t f = 0; f < kFnCount; ++f) {
        if (canonical == kFns[f].name) return f;
    }
    return -1;
}

int Expression::binaryCode(const std::string& op) {
    static const char* const kOps[] = {"+", "-", "*", "/", "^", "<", "<=", ">", ">=", "==", "!=", "%"};
    static_assert(sizeof(kOps) / sizeof(kOps[0]) == kFmodOp + 1, "every operator has a name");
    for (int b = 0; b <= kFmodOp; ++b) {
        if (op == kOps[b]) return b;
    }
    return -1;
}

void Expression::strandVariablesInto(const StrandSet& strands, std::size_t s, const ScalpMesh* scalp, const Bound& bound,
                                     float* vars) const {
    Env env;
    strandVariables(env, strands, s, scalp, bound);
    std::copy(env.vars.begin(), env.vars.end(), vars);
}

float Expression::evaluate(const StrandSet& strands, std::size_t s, const ScalpMesh* scalp, const Bound* bound) const {
    Env env;
    Bound local;
    if (!bound) {
        local = bind(strands);
        bound = &local;
    }
    strandVariables(env, strands, s, scalp, *bound);
    return run(env);
}

Vec3f Expression::evaluateVector(const StrandSet& strands, std::size_t s, const ScalpMesh* scalp, const Bound* bound) const {
    Env env;
    Bound local;
    if (!bound) {
        local = bind(strands);
        bound = &local;
    }
    strandVariables(env, strands, s, scalp, *bound);
    return runVector(env);
}

// The per-strand setup of the last strand evaluated per point on this thread:
// its environment and the arc length at each of its points, so a strand's
// points cost one strand setup between them. Keyed on the binding (its serial:
// addresses are reused once freed, a serial never is), the set, the strand,
// its point count and its point array (copies of a set share it until written).
struct Expression::StrandCache {
    const Expression* expression = nullptr;
    const StrandSet* strands = nullptr;
    std::uint64_t bound = 0;  // Bound::serial
    std::size_t strand = 0;
    std::size_t count = 0;
    const Vec3f* points = nullptr;
    Env env;
    std::vector<float> arc;  // cumulative length at each point, over the total (0 to 1)
};

Expression::Env Expression::pointEnv(const StrandSet& strands, std::size_t s, std::size_t point, const ScalpMesh* scalp,
                                     const Bound* bound) const {
    Bound local;
    if (!bound) {
        local = bind(strands);
        bound = &local;
    }
    const std::size_t n = strands.strandPointCount(s);
    const Vec3f* pts = strands.strandPoints(s);
    thread_local StrandCache cache;
    const bool cached = cache.bound == bound->serial && cache.expression == this && cache.strands == &strands &&
                        cache.strand == s && cache.count == n && cache.points == pts;
    if (!cached) {
        cache.expression = this;
        cache.strands = &strands;
        cache.bound = bound->serial;
        cache.strand = s;
        cache.count = n;
        cache.points = pts;
        cache.env = Env{};
        strandVariables(cache.env, strands, s, scalp, *bound);
        if (readsPoint_) {
            cache.arc.resize(n);  // grows once per thread, then stays
            float total = 0.0f;
            for (std::size_t i = 0; i < n; ++i) {
                if (i > 0) total += (pts[i] - pts[i - 1]).length();
                cache.arc[i] = total;
            }
            if (total > 1e-12f) {
                for (float& a : cache.arc) a /= total;
            } else {
                for (std::size_t i = 0; i < n; ++i) cache.arc[i] = n > 1 ? float(i) / float(n - 1) : 0.0f;
            }
        }
    }
    Env env = cache.env;
    env.channels = &bound->channels;  // this binding's (the cached one may have been a copy since freed)
    env.pointChannels = &bound->pointChannels;
    if (point < n) {
        env.point = strands.strandBegin(s) + point;
        if (readsPoint_) {
            env.vars[kT] = cache.arc[point];
            env.vars[kPT] = n > 1 ? float(point) / float(n - 1) : 0.0f;
            env.vars[kPX] = pts[point].x;
            env.vars[kPY] = pts[point].y;
            env.vars[kPZ] = pts[point].z;
            env.vars[kPointIndex] = static_cast<float>(point);
        }
    }
    return env;
}

float Expression::evaluatePoint(const StrandSet& strands, std::size_t s, std::size_t point, const ScalpMesh* scalp,
                                const Bound* bound) const {
    return run(pointEnv(strands, s, point, scalp, bound));
}

Vec3f Expression::evaluatePointVector(const StrandSet& strands, std::size_t s, std::size_t point, const ScalpMesh* scalp,
                                      const Bound* bound) const {
    return runVector(pointEnv(strands, s, point, scalp, bound));
}

void Expression::rootEnv(Env& env, const StrandRoot& root, const ScalpMesh* scalp) const {
    rootVariables(env.vars, root, scalp, Vec3f{}, readsRef_);
    std::uint64_t h = hash64(static_cast<std::uint64_t>(static_cast<std::int64_t>(root.face)));
    std::uint32_t bits = 0;
    std::memcpy(&bits, &root.u, sizeof bits);
    h = hashCombine(h, bits);
    std::memcpy(&bits, &root.v, sizeof bits);
    h = hashCombine(h, bits);
    env.vars[kRand] = unitFloat(h);
    env.vars[kFaceId] = static_cast<float>(root.face);
    if (readsTime_) {
        env.vars[kFrame] = static_cast<float>(frame_);
        env.vars[kTime] = static_cast<float>(seconds_);
    }
    env.root = &root;
}

float Expression::evaluateAt(const StrandRoot& root, const ScalpMesh* scalp) const {
    Env env;
    rootEnv(env, root, scalp);
    return run(env);
}

Vec3f Expression::evaluateVectorAt(const StrandRoot& root, const ScalpMesh* scalp) const {
    Env env;
    rootEnv(env, root, scalp);
    return runVector(env);
}

}  // namespace groomlab
