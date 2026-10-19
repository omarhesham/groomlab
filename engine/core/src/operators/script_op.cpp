// SPDX-License-Identifier: Apache-2.0
// Script nodes (docs/CUSTOM_NODES.md, phase 2; script.h, node_library.h): a node whose work is a
// script, read from its definition, a .glnode file the node holds (a groom carries it, as it
// carries a compound node's copy):
//
//   {"name": "wave", "type": "Script", "params": {
//       "definition": {"glnode": 1, "id": "groomlab.wave", ..., "params": [...], "writes": ["P"],
//                      "script": "per point { ... }"},
//       "amplitude": 0.4}}
//
// Its params are the header's (each a real spec: a mappable one takes maps, expressions and
// fields as any node's). The script compiles once per definition (again per evaluation only when
// it reads the time or a map, whose values it binds when compiled). Strands run side by side,
// each alone, so an edit upstream that changed some strands makes only those again
// (evaluateIncremental); a script writing no P hands on its input's very points (the graph sees
// the points kept, as after a Width edit). A definition that cannot be read or compiled hands the
// strands on with a note saying why (a groom never fails to open by it); a script running past
// its step budget is the node's error.

#include <memory>
#include <mutex>

#include <nlohmann/json.hpp>

#include "groomlab/export_view.h"
#include "groomlab/field.h"
#include "groomlab/graph.h"
#include "groomlab/node_library.h"
#include "groomlab/operators.h"
#include "groomlab/script.h"
#include "incremental.h"
#include "internal.h"

namespace groomlab {

namespace {

class ScriptOp : public Operator {
public:
    std::string typeName() const override { return op::kScript; }
    std::string category() const override { return "shape"; }
    std::vector<ParamSpec> paramSpecs() const override {
        return {ParamSpec{"definition", std::string("{}"), "The script node's file: its header and its script (JSON)"}.noInput()};
    }
    std::vector<ParamSpec> extraParamSpecs() const override {
        const auto d = definition();
        return d ? d->header.params : std::vector<ParamSpec>{};
    }
    std::vector<PortSpec> inputs() const override {
        const auto d = definition();
        if (d) return d->header.inputs;
        return {{"strands", PortType::Strands}};
    }
    std::vector<std::string> externalInputs() const override { return {"scalp"}; }
    // A script reading the time or a map (expressionSalt), as an expression's.
    std::uint64_t contextSalt(const EvalContext& ctx) const override {
        const auto d = definition();
        return d ? expressionSalt(d->header.script, ctx) : 0;
    }
    std::vector<ChannelWrite> channelWrites() const override {
        std::vector<ChannelWrite> out;
        const auto d = definition();
        if (!d) return out;
        for (const std::string& w : d->header.writes) {
            if (w.rfind("channel.", 0) == 0) out.push_back({w.substr(8), ""});
            if (w == "color") {
                for (const char* c : kColorChannels) out.push_back({c, ""});
            }
        }
        return out;
    }

    Value evaluate(const EvalContext& ctx, const std::vector<Value>& inputs) const override {
        Value out;
        run(ctx, inputs, nullptr, &out, nullptr);
        return out;
    }
    bool evaluateIncremental(const EvalContext& ctx, const std::vector<Value>& inputs, const IncrementalInputs& incremental,
                             Value* out, StrandChanges* changed) const override {
        return run(ctx, inputs, &incremental, out, changed);
    }

private:
    struct Definition {
        ScriptNodeHeader header;
        std::string name;  // its label (or id), for messages
        std::shared_ptr<const Script> compiled;  // null: compiled per evaluation (time, maps)
    };

    // In full, or (with `incremental`) only the strands the input changed in, into the last
    // result's copy; false when that cannot be done.
    bool run(const EvalContext& ctx, const std::vector<Value>& inputs, const IncrementalInputs* incremental, Value* result,
             StrandChanges* changed) const {
        const auto d = definition();
        if (!d) {
            if (incremental) return false;
            // Nothing to run: its strands handed on, with why.
            ctx.note("Error: script node '" + failedName() + "' cannot run and hands its strands on: " + definitionError());
            *result = inputs.empty() ? Value{} : inputs.front();
            return true;
        }
        const StrandsPtr in = requireStrands(inputs, d->name.c_str());
        std::shared_ptr<const Script> script = d->compiled;
        if (!script) {
            try {
                script = std::make_shared<const Script>(d->header.script, d->header.scriptParams, d->header.writes);
            } catch (const ScriptError& e) {
                if (incremental) return false;
                ctx.note("Error: script node '" + d->name + "' cannot run and hands its strands on: " + e.what());
                *result = in;
                return true;
            }
        }

        // The params' values: as set, else the header's defaults, within their limits.
        ParamSet values = params();
        auto limits = std::make_shared<std::map<std::string, ParamSet::Limits>>();
        for (const ParamSpec& spec : d->header.params) {
            if (!values.has(spec.name)) values.set(spec.name, spec.defaultValue);
            if (spec.min || spec.max) limits->emplace(spec.name, ParamSet::Limits{spec.min, spec.max});
        }
        values.setLimits(limits);
        const ScalpMesh* scalp = scalpOf(ctx);
        const GuidesPtr guides = inputs.size() > 1 ? valueAs<GuidesPtr>(inputs[1]) : nullptr;
        Script::Inputs given;
        given.scalp = scalp;
        given.guides = guides.get();
        std::vector<std::unique_ptr<ScalarField>> fields;
        std::vector<Ramp> ramps;
        ramps.reserve(d->header.params.size());  // (pointers into it are handed on)
        for (std::size_t i = 0; i < d->header.params.size(); ++i) {
            const ParamSpec& spec = d->header.params[i];
            Script::Value v;
            switch (d->header.scriptParams[i].kind) {
                case ScriptParam::Kind::Number:
                    if (paramMappable(spec)) {
                        fields.push_back(std::make_unique<ScalarField>(values, spec.name, scalp, guides.get()));
                        fields.back()->validate(*in);
                        v.field = fields.back().get();
                    } else {
                        v.value.x = static_cast<float>(values.getDouble(spec.name));
                    }
                    break;
                case ScriptParam::Kind::Int: v.value.x = static_cast<float>(values.getInt(spec.name)); break;
                case ScriptParam::Kind::Bool: v.value.x = values.getBool(spec.name) ? 1.0f : 0.0f; break;
                case ScriptParam::Kind::Choice: {
                    const std::string choice = values.getString(spec.name);
                    const auto at = std::find(spec.choices.begin(), spec.choices.end(), choice);
                    v.value.x = at == spec.choices.end() ? 0.0f : static_cast<float>(at - spec.choices.begin());
                    break;
                }
                case ScriptParam::Kind::Color: v.value = values.getVec3(spec.name); break;
                case ScriptParam::Kind::Ramp:
                    ramps.push_back(values.getRamp(spec.name));
                    v.ramp = &ramps.back();
                    break;
            }
            given.params.push_back(v);
        }

        const StrandChanges* changes = nullptr;
        StrandsPtr last;  // the last result, to make again only the strands the input changed in
        if (incremental) {
            changes = someStrandsChanged(*incremental, 0);
            last = changes ? previousStrandsFor(*incremental, *in) : nullptr;
            if (!last || script->readsLongest()) return false;
            for (const auto& f : fields) {
                if (!f->strandLocal()) return false;
            }
            if (script->readsGuides() && d->header.inputs.size() > 1 && !inputUnchanged(*incremental, 1)) return false;
            if (changes->none()) {  // the same strands in: the same result
                *result = last;
                *changed = *changes;
                return true;
            }
        } else {
            // A channel it reads by name that does not reach it reads 0: likely a typo.
            for (const std::string& name : script->channelsRead()) {
                if (!in->strandChannel(name) && !in->pointChannel(name)) {
                    ctx.note(d->name + " reads '" + name + "', which is no input, param or channel of the strands coming in (it reads 0)");
                }
            }
        }
        auto out = std::make_shared<StrandSet>(*in);
        if (last) {
            // The strands left alone as they were made: every array the script writes, the last
            // result's.
            if (script->writesPoints()) positionsOfLast(*out, last, *incremental);
            if (script->writesWidths()) {
                if (last->widths().size() != last->pointCount()) return false;
                widthsOfLast(*out, last, *incremental);
            }
            if (script->writesColors()) {
                for (const char* c : kColorChannels) {
                    if (!out->copyPointChannelFrom(c, *last) || !out->copyStrandChannelFrom(c, *last)) return false;
                }
            }
            for (const std::string& c : script->strandChannels()) {
                if (!out->copyStrandChannelFrom(c, *last)) return false;
            }
            for (const std::string& c : script->pointChannels()) {
                if (!out->copyPointChannelFrom(c, *last)) return false;
            }
        }
        try {
            script->run(*out, *in, given, last ? &changes->strands : nullptr);
        } catch (const ScriptError& e) {
            throw GraphError(d->name + ": " + e.what());
        }
        if (last) *changed = *changes;
        *result = StrandsPtr(out);
        return true;
    }

    std::shared_ptr<const Definition> definition() const {
        const std::string text = params().getString("definition");
        std::lock_guard<std::mutex> lock(mutex_);
        if (text != parsedText_) {
            parsedText_ = text;
            parsed_.reset();
            error_.clear();
            name_.clear();
            const nlohmann::json file = nlohmann::json::parse(text, nullptr, false);
            if (file.is_object()) {
                const std::string label = file.value("label", nlohmann::json()).is_string() ? file["label"].get<std::string>() : "";
                const std::string id = file.value("id", nlohmann::json()).is_string() ? file["id"].get<std::string>() : "";
                name_ = !label.empty() ? label : (!id.empty() ? id : std::string("Script"));
            }
            try {
                if (!file.is_object() || !file.contains("script")) throw GraphError("its definition holds no script");
                const nlohmann::json format = file.value("glnode", nlohmann::json(kNodeFileFormat));
                if (format.is_number_integer() && format.get<int>() > kNodeFileFormat) {
                    throw GraphError("it is of node file format " + format.dump() + ", newer than this GroomLab reads: update GroomLab");
                }
                auto d = std::make_shared<Definition>();
                d->header = scriptNodeHeader(text);
                d->name = name_;
                // Compiled now (also to find its errors); kept unless it reads what changes
                // from one evaluation to the next (the time, a map's values). Maps are found
                // when it evaluates (here any name will do).
                const ExpressionEnvironmentScope none(nullptr);
                auto compiled = std::make_shared<const Script>(d->header.script, d->header.scriptParams, d->header.writes);
                const ExpressionDependencies reads = scanExpression(d->header.script);
                if (!reads.time && reads.maps.empty()) d->compiled = std::move(compiled);
                parsed_ = std::move(d);
            } catch (const std::exception& e) {
                error_ = e.what();
            }
        }
        return parsed_;
    }
    std::string definitionError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_.empty() ? "no definition" : error_;
    }
    std::string failedName() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return name_.empty() ? std::string("Script") : name_;
    }

    mutable std::mutex mutex_;
    mutable std::string parsedText_;
    mutable std::shared_ptr<const Definition> parsed_;
    mutable std::string error_;
    mutable std::string name_;
};

}  // namespace

void registerScriptOperator(OperatorRegistry& registry) {
    registry.registerType(op::kScript,
                          {"utility", "A script node: a script in the expression language run per strand and per point.",
                           "script custom code", "", true},
                          [] { return std::make_unique<ScriptOp>(); });
}

}  // namespace groomlab
