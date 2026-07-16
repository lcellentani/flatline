#pragma once
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "gm_applier.hpp"
#include "gm_effects.hpp"
#include "gm_gauntlet.hpp"
#include "gm_json.hpp"
#include "gm_roll.hpp"
#include "gm_state.hpp"

// GM Adjudicator — v0 replay logger.
// Closes the last seam marked in wizard_of_oz_demo.cpp since session log 3
// and named the sole remaining piece in session log 7 §4: "log the raw
// work order plus the applied deltas and the RNG seed/result as one
// replayable event" (gm-adjudicator.md §4).
//
// One LogEntry per TURN, not per gauntlet stage (locked in conversation):
// the gauntlet's internal stages collapse into a single field on a
// GauntletRejected outcome. One WorkOrder call = one turn = one entry,
// mirroring the "one call per turn" decision gm_effects.hpp already locks
// for WorkOrder itself.
//
// Approach B (locked): every entry is self-contained. An Applied entry
// carries world_before AND world_after, not just deltas — so any single
// turn is independently replayable/verifiable without first replaying
// every turn that came before it. Costs one duplicate World snapshot per
// turn (world_after[n] == world_before[n+1]); accepted at this project's
// scale, named here as the real tradeoff it is.
//
// New serializers for RollResult/Failure/ModifierBudgetFailure live HERE,
// not in gm_roll.hpp/gm_gauntlet.hpp. Two reasons: (1) gm_roll.hpp already
// includes gm_json.hpp for the seed's canonical dump, so gm_json.hpp
// including gm_roll.hpp back would cycle; (2) gm_roll.hpp's own header
// comment leans on RollResult being a pure, I/O-free struct — this header
// sits downstream of everything and is the natural place for log-event-
// specific JSON, without touching any existing header's contract.
//
// Outcome is a std::variant<ModifierBudgetRejected, GauntletRejected,
// Applied>, not a flat struct with "present only if" fields — locked
// specifically to make invalid reads (e.g. touching world_after on a
// rejected turn) structurally impossible rather than a convention to
// remember at every call site. Same fix shape as Damage.via going from
// optional-with-a-silent-bypass to required-with-a-sentinel, and the same
// reasoning gm_gauntlet.hpp already used to split Failure from
// ModifierBudgetFailure instead of folding them into one type.

// ---------------------------------------------------------------------
// RollResult's serializer must live in gm::roll (its own namespace), not
// gm::log — nlohmann's to_json/from_json hooks are found via ADL on the
// TYPE's associated namespace, not wherever the function happens to be
// defined. Reopening gm::roll here is additive (doesn't edit gm_roll.hpp
// itself, doesn't add a dependency the roll header doesn't already have)
// and is the standard idiom for hooking ADL from a downstream header.
// ---------------------------------------------------------------------
namespace gm::roll {
using json = nlohmann::json;
inline void to_json(json& j, const RollResult& r) {
    j = json{{"die", r.die}, {"modifier_sum", r.modifier_sum}, {"total", r.total}, {"success", r.success}};
}
inline void from_json(const json& j, RollResult& r) {
    j.at("die").get_to(r.die);
    j.at("modifier_sum").get_to(r.modifier_sum);
    j.at("total").get_to(r.total);
    j.at("success").get_to(r.success);
}
}  // namespace gm::roll

// ---------------------------------------------------------------------
// VitalTransition's serializer lives in gm::applier (its own namespace),
// same ADL reasoning as RollResult above, reopened here rather than
// editing gm_applier.hpp itself.
// ---------------------------------------------------------------------
namespace gm::applier {
using json = nlohmann::json;
inline void to_json(json& j, const VitalTransition& t) {
    j = json{{"entity", t.entity},
             {"from", gm::vital_state_label(t.from)},
             {"to", gm::vital_state_label(t.to)},
             {"overkill", t.overkill}};
}
inline void from_json(const json& j, VitalTransition& t) {
    j.at("entity").get_to(t.entity);
    t.from = gm::vital_state_from(j.at("from"));
    t.to = gm::vital_state_from(j.at("to"));
    j.at("overkill").get_to(t.overkill);
}
}  // namespace gm::applier

namespace gm::log {

using json = nlohmann::json;

// ---------------------------------------------------------------------
// A roll that actually happened this turn. Absent (std::optional) on
// no-roll turns (WorkOrder.modifiers empty, success automatic) — logging
// a phantom seed/die for a turn that never rolled would misrepresent the
// "the roll picks a branch, it does not author one" guarantee the spec
// already makes.
// ---------------------------------------------------------------------
struct RollInfo {
    uint64_t seed;
    roll::RollResult result;
};

// ---------------------------------------------------------------------
// The three outcomes a turn can reach. Rejected turns never roll a
// branch selection or touch the applier — GauntletRejected still CAN
// carry a RollInfo (a turn can roll, then fail the gauntlet on the branch
// the roll selected), but ModifierBudgetRejected structurally cannot: the
// pipeline never reaches the roll before that gate fires.
// ---------------------------------------------------------------------
struct ModifierBudgetRejected {
    int64_t magnitude_sum;
    int64_t budget;
    std::string reason;
};

struct GauntletRejected {
    std::optional<RollInfo> roll;
    std::size_t effect_index;
    std::string stage;  // gauntlet::stage_name(f.stage)
    std::string reason;
    // Present only when stage == "batch_conflict" (gauntlet::Failure's own
    // field, session log 11) — the earlier effect index in the same branch
    // responsible. Same omitted-not-null idiom as `roll` above: a rejection
    // at any other stage structurally has no earlier-effect-in-this-batch
    // to point at.
    std::optional<std::size_t> conflicting_with_index;
};

struct Applied {
    std::optional<RollInfo> roll;
    std::vector<Effect> applied_effects;  // the gauntlet-approved, roll-selected branch, post-clamp
    std::string narration;
    World world_before;
    World world_after;
    // Empty on most turns (no vital-state change). Populated by the caller
    // via applier::vital_transitions(world_before, applied_effects) — not
    // derived here or reconstructed from world_before/world_after later;
    // see that function's own comment for why storing beats re-deriving.
    std::vector<applier::VitalTransition> vital_transitions;
};

using Outcome = std::variant<ModifierBudgetRejected, GauntletRejected, Applied>;

struct LogEntry {
    int64_t turn_number;
    WorkOrder work_order;  // raw — reuses gm_json.hpp's existing to_json/from_json verbatim
    Outcome outcome;
};

// ---------------------------------------------------------------------
// JSON: RollInfo (RollResult's own serializer is reopened into gm::roll
// above, for ADL — see that block's comment)
// ---------------------------------------------------------------------
inline void to_json(json& j, const RollInfo& r) { j = json{{"seed", r.seed}, {"result", r.result}}; }
inline void from_json(const json& j, RollInfo& r) {
    j.at("seed").get_to(r.seed);
    j.at("result").get_to(r.result);
}

// ---------------------------------------------------------------------
// JSON: the three Outcome alternatives, plus the variant's own tag
// dispatch on "status" — same recipe gm_json.hpp already uses for Effect
// and Ruling, not a new one.
// ---------------------------------------------------------------------
inline void to_json(json& j, const ModifierBudgetRejected& o) {
    j = json{{"status", "modifier_budget_rejected"},
             {"magnitude_sum", o.magnitude_sum},
             {"budget", o.budget},
             {"reason", o.reason}};
}
inline void from_json(const json& j, ModifierBudgetRejected& o) {
    j.at("magnitude_sum").get_to(o.magnitude_sum);
    j.at("budget").get_to(o.budget);
    j.at("reason").get_to(o.reason);
}

inline void to_json(json& j, const GauntletRejected& o) {
    j = json::object();
    j["status"] = "gauntlet_rejected";
    if (o.roll) j["roll"] = *o.roll;  // omitted entirely, not null, when this turn never rolled
    j["effect_index"] = o.effect_index;
    j["stage"] = o.stage;
    j["reason"] = o.reason;
    if (o.conflicting_with_index) j["conflicting_with_index"] = *o.conflicting_with_index;
}
inline void from_json(const json& j, GauntletRejected& o) {
    if (j.contains("roll"))
        o.roll = j.at("roll").get<RollInfo>();
    else
        o.roll = std::nullopt;
    j.at("effect_index").get_to(o.effect_index);
    j.at("stage").get_to(o.stage);
    j.at("reason").get_to(o.reason);
    // Tolerant of omission both ways: absent on non-conflict rejections, and
    // on any log line written before this field existed (session log 11).
    if (j.contains("conflicting_with_index"))
        o.conflicting_with_index = j.at("conflicting_with_index").get<std::size_t>();
    else
        o.conflicting_with_index = std::nullopt;
}

inline void to_json(json& j, const Applied& o) {
    j = json::object();
    j["status"] = "applied";
    if (o.roll) j["roll"] = *o.roll;
    j["applied_effects"] = o.applied_effects;
    j["narration"] = o.narration;
    j["world_before"] = o.world_before;
    j["world_after"] = o.world_after;
    j["vital_transitions"] = o.vital_transitions;  // may be an empty array
}
inline void from_json(const json& j, Applied& o) {
    if (j.contains("roll"))
        o.roll = j.at("roll").get<RollInfo>();
    else
        o.roll = std::nullopt;
    j.at("applied_effects").get_to(o.applied_effects);
    j.at("narration").get_to(o.narration);
    j.at("world_before").get_to(o.world_before);
    j.at("world_after").get_to(o.world_after);
    // Tolerant of omission: logs written before this field existed still parse.
    o.vital_transitions = j.value("vital_transitions", std::vector<applier::VitalTransition>{});
}

inline void to_json(json& j, const Outcome& o) {
    std::visit([&](const auto& alt) { to_json(j, alt); }, o);
}
inline void from_json(const json& j, Outcome& o) {
    const auto status = j.at("status").get<std::string>();
    if (status == "modifier_budget_rejected")
        o = j.get<ModifierBudgetRejected>();
    else if (status == "gauntlet_rejected")
        o = j.get<GauntletRejected>();
    else if (status == "applied")
        o = j.get<Applied>();
    else
        throw std::runtime_error("unknown log entry status '" + status + "'");
}

// ---------------------------------------------------------------------
// JSON: LogEntry
// ---------------------------------------------------------------------
inline void to_json(json& j, const LogEntry& e) {
    j = json::object();
    j["turn_number"] = e.turn_number;
    j["work_order"] = e.work_order;
    // Flatten the outcome's fields onto the entry rather than nesting under
    // an "outcome" key — keeps one status-tagged JSON object per line
    // instead of an object-within-an-object, cheaper to grep/skim on disk.
    json outcome_json;
    to_json(outcome_json, e.outcome);
    for (auto it = outcome_json.begin(); it != outcome_json.end(); ++it) j[it.key()] = it.value();
}
inline void from_json(const json& j, LogEntry& e) {
    j.at("turn_number").get_to(e.turn_number);
    j.at("work_order").get_to(e.work_order);
    j.get_to(e.outcome);  // Outcome::from_json reads "status" off the same flattened object
}

// ---------------------------------------------------------------------
// Convenience: JSON Lines append/read. One entry per turn (locked): the
// gauntlet's internal stages never get their own line, only the turn's
// final outcome does. Append, not rewrite — matches the spec's own verb
// and survives a crash mid-session with every prior line intact.
// ---------------------------------------------------------------------
inline void append_to_file(const std::string& path, const LogEntry& entry) {
    std::ofstream f(path, std::ios::app);
    if (!f) throw std::runtime_error("gm::log::append_to_file: could not open '" + path + "'");
    f << json(entry).dump() << "\n";
}

inline std::vector<LogEntry> read_log(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("gm::log::read_log: could not open '" + path + "'");
    std::vector<LogEntry> entries;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        entries.push_back(json::parse(line).get<LogEntry>());
    }
    return entries;
}

// ---------------------------------------------------------------------
// The actual replay guarantee, not just storage: re-run apply() on the
// entry's own recorded inputs and confirm it reproduces the entry's own
// recorded output. Takes an Applied directly (caller extracts via
// std::get_if) rather than a whole LogEntry — same "trust the caller's
// precondition, don't re-derive upstream state" posture gm_applier.hpp
// already takes toward run_gauntlet's output. Only Applied entries have
// anything to replay; rejected turns never reached the applier.
//
// Comparison is by JSON equality (json(a) == json(b)), not a hand-written
// operator== on World — same trick the demo's own round-trip proof
// already uses, rather than adding equality operators to every state
// struct just for this one check.
// ---------------------------------------------------------------------
inline bool replay_matches(const Applied& applied) {
    World replayed = applier::apply(applied.world_before, applied.applied_effects);
    return json(replayed) == json(applied.world_after);
}

}  // namespace gm::log
