// GM Adjudicator — Wizard-of-Oz loop skeleton (Experiment 1).
//
// Demonstrates the full turn loop end to end: serialization bridge,
// modifier budget, seeded roll, validator gauntlet, applier, AND now the
// logger -- the last seam named since session log 3 is closed. Build:
//   g++ -std=c++20 -I. -I<path-to-nlohmann> wizard_of_oz_demo.cpp -o woz
//
// The real Experiment-1 loop replaces the hard-coded reads below with:
//   load room -> print state to GM -> read player intent -> GM writes work
//   order JSON -> [BRIDGE] -> [MODIFIER BUDGET] -> [ROLL] -> [VALIDATOR] ->
//   [APPLY] -> [LOG] -> repeat.
//
// [ROLL]: a fixed d20+DC scheme (gm_roll.hpp) resolves wo.modifiers into an
// actual branch selection. [MODIFIER BUDGET] is a stage ahead of the roll
// (gm_gauntlet.hpp's validate_modifiers) -- it has to run before the roll,
// not after, since an unbounded modifier list could otherwise force the
// roll's outcome (see gm_rules.hpp's kModifierBudget comment).
//
// [LOG] (gm_log.hpp, session log 8): exactly ONE entry per turn, regardless
// of which of the three exit points this loop iteration took -- the
// gauntlet's internal per-effect stages never get their own log line, only
// the turn's final outcome does (locked in discussion: "one WorkOrder call
// = one turn = one entry"). Every entry is self-contained (Approach B):
// an Applied entry carries world_before AND world_after, so any single
// logged turn is independently replayable without first replaying every
// turn before it. See the log::replay_matches() proof in test_log.cpp for
// the property this earns: re-running apply() on a logged entry's own
// inputs reproduces its own recorded output.

#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>

#include "gm_gauntlet.hpp"
#include "gm_json.hpp"
#include "gm_log.hpp"
#include "gm_roll.hpp"
#include "gm_turn.hpp"

static const char* kLogPath = "woz_log.jsonl";

static std::string slurp(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main() {
    using namespace gm;

    // 1. Engine loads the room. (In the loop, this is authored once per scenario.)
    World world = parse_world(slurp("room_guardroom.json"));

    // 2. Show state to the human GM. This is the "render for the LLM" step —
    //    the same view a model would receive as prompt context.
    std::cout << "=== STATE SHOWN TO GM ===\n" << dump(world) << "\n\n";

    // 3. GM reads player intent ("I bribe the guard with 20 gold") and authors
    //    a work order as JSON. [BRIDGE] parses it into typed effects.
    WorkOrder wo = parse_work_order(slurp("workorder_bribe.json"));

    std::cout << "=== PARSED WORK ORDER ===\n";
    std::cout << "interpretation : " << wo.interpretation << "\n";
    std::cout << "ruling         : " << json(wo.ruling).get<std::string>() << "\n";
    std::cout << "modifiers      : " << wo.modifiers.size() << " (=> engine rolls)\n";
    std::cout << "on success     : " << wo.effects_on_success.size() << " effects\n";
    std::cout << "on failure     : " << wo.effects_on_failure.size() << " effects\n\n";

    // 4. [MODIFIER BUDGET] wo.modifiers must fit kModifierBudget (gm_rules.hpp)
    //    BEFORE the roll runs — an unbounded modifier list could otherwise
    //    force the roll's outcome regardless of what the die shows, which is
    //    exactly the "engine owns the dice" guarantee this stage protects.
    //    turn_number is engine-tracked, not a WorkOrder field (gm_roll.hpp) —
    //    hardcoded here since this demo runs a single fixed turn.
    // Steps 4-6c (modifier budget -> roll -> gauntlet -> apply -> log) are
    // now one call into the shared pipeline (gm_turn.hpp, Phase 1) -- the
    // demo's only remaining job here is reconstructing the same console
    // narration from the returned Outcome, since process_turn itself does
    // no I/O (that's what keeps it reusable by woz_loop/woz_gui later).
    const int64_t turn_number = 1;
    auto [world_after, outcome] = turn::process_turn(world, turn_number, wo, kLogPath);

    if (auto* budget_failure = std::get_if<log::ModifierBudgetRejected>(&outcome)) {
        std::cout << "=== MODIFIER BUDGET ===\nREJECTED: " << budget_failure->reason << "\n\n";
    } else {
        std::cout << "=== MODIFIER BUDGET ===\nOK (magnitude sum within kModifierBudget)\n\n";

        bool rolled = !wo.modifiers.empty();
        const std::optional<log::RollInfo>* roll_info = std::holds_alternative<log::GauntletRejected>(outcome)
                                                            ? &std::get<log::GauntletRejected>(outcome).roll
                                                            : &std::get<log::Applied>(outcome).roll;

        bool success = true;
        if (rolled && *roll_info) {
            const auto& r = (*roll_info)->result;
            success = r.success;
            std::cout << "=== ROLL ===\n"
                      << "die " << r.die << " + modifiers " << r.modifier_sum << " = " << r.total << " vs DC "
                      << rules::kDifficultyClass << " -> " << (success ? "SUCCESS" : "FAILURE") << "\n\n";
        } else {
            std::cout << "=== ROLL ===\nno modifiers -> no roll -> automatic success\n\n";
        }

        std::cout << "=== GAUNTLET (" << (success ? "effects_on_success" : "effects_on_failure") << ") ===\n";
        if (auto* applied = std::get_if<log::Applied>(&outcome)) {
            std::cout << applied->applied_effects.size() << " effect(s) survived the gauntlet, ready for [APPLY]\n\n";
            std::cout << "=== NARRATION ===\n" << applied->narration << "\n\n";
            std::cout << "=== APPLIED (world before -> after) ===\n";
            std::cout << "player.gold : " << world.player.gold << " -> " << world_after.player.gold << "\n";
            std::cout << "player.inventory size : " << world.player.inventory.size() << " -> "
                      << world_after.player.inventory.size() << "\n\n";
        } else {
            const auto& f = std::get<log::GauntletRejected>(outcome);
            std::cout << "REJECTED at effect " << f.effect_index << " [" << f.stage << "]: " << f.reason << "\n\n";
        }
    }

    // 7. Round-trip proof: parse -> struct -> serialize is faithful. This is the
    //    property that lets the log replay a run and lets a human and a model
    //    share one format.
    std::string reserialized = dump(wo);
    bool roundtrip_ok = json::parse(reserialized) == json::parse(slurp("workorder_bribe.json"));
    std::cout << "round-trip identical: " << (roundtrip_ok ? "yes" : "NO") << "\n";

    // 8. Closed-vocabulary gate: an invented verb is rejected AT THE BRIDGE,
    //    before it could ever reach the validator.
    try {
        parse_work_order(R"({"interpretation":"x","ruling":"allow",
            "effects_on_success":[{"type":"teleport","who":"player"}]})");
        std::cout << "vocabulary gate: NOT enforced (bug)\n";
    } catch (const std::exception& ex) {
        std::cout << "vocabulary gate: rejected -> " << ex.what() << "\n";
    }

    // 9. The spec's own "hallucination (deny at the gauntlet)" worked example,
    //    run for real: schema-valid JSON on disk -> [BRIDGE] parses cleanly
    //    (silver_key is a well-formed id, just not a real one) -> [VALIDATOR]
    //    is the stage that actually catches it, one step later than the
    //    vocabulary gate above.
    WorkOrder hallucinated = parse_work_order(slurp("workorder_hallucinated.json"));
    auto hallucination_result = gauntlet::run_gauntlet(world, hallucinated.effects_on_success);
    bool hallucination_caught = std::holds_alternative<gauntlet::Failure>(hallucination_result);
    std::cout << "hallucination worked example: "
              << (hallucination_caught ? "rejected by gauntlet (correct)\n" : "NOT caught (bug)\n");

    return (roundtrip_ok && hallucination_caught) ? 0 : 1;
}
