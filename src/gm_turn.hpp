#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gm_applier.hpp"
#include "gm_effects.hpp"
#include "gm_gauntlet.hpp"
#include "gm_log.hpp"
#include "gm_roll.hpp"
#include "gm_state.hpp"

// GM Adjudicator — shared per-turn pipeline (Phase 1, gm-multiturn-loop.md).
//
// Aggregates the four already-proven, already-tested stages wizard_of_oz_demo.cpp
// ran inline before this header existed -- [MODIFIER BUDGET] -> [ROLL] ->
// [VALIDATOR] -> [APPLY] -> [LOG] -- into one function, so the demo, the CLI
// loop (woz_loop.cpp, Phase 2), and the GUI tool (woz_gui.cpp, Phase 3) all
// call one implementation instead of forking it. No new logic: every call
// here is unchanged from gm_gauntlet.hpp/gm_roll.hpp/gm_applier.hpp/gm_log.hpp.
//
// Deliberately does no console/UI output -- it returns {World, log::Outcome}
// and lets the caller render whatever it needs from Outcome (matches
// gm_log.hpp's own reasoning for reusing Outcome as the report shape: "every
// field a console or GUI renderer needs already exists there"). This is what
// keeps the same function usable by a stdout demo and a Dear ImGui panel.
//
// One call = one turn = one gm::log::LogEntry appended, regardless of which
// of the three outcomes this call reaches -- same invariant gm_log.hpp locks.

namespace gm::turn {

inline std::pair<World, log::Outcome> process_turn(const World& world, int64_t turn_number, const WorkOrder& wo,
                                                   const std::string& log_path) {
    // [MODIFIER BUDGET] -- must run before the roll (see gm_rules.hpp's
    // kModifierBudget comment): an unbounded modifier list could otherwise
    // force the roll's outcome regardless of what the die shows.
    if (auto budget_failure = gauntlet::validate_modifiers(wo.modifiers)) {
        log::Outcome outcome =
            log::ModifierBudgetRejected{budget_failure->magnitude_sum, budget_failure->budget, budget_failure->reason};
        log::append_to_file(log_path, log::LogEntry{turn_number, wo, outcome});
        return {world, outcome};
    }

    // [ROLL] -- modifiers empty => no roll, success is automatic (locked,
    // gm_effects.hpp WorkOrder comment). Otherwise the seeded d20+DC roll
    // (gm_roll.hpp) picks the branch for real.
    bool rolled = !wo.modifiers.empty();
    bool success = true;
    std::optional<log::RollInfo> roll_info;
    if (rolled) {
        uint64_t seed = roll::seed_for_turn(turn_number, wo);
        roll::RollResult r = roll::resolve_turn(turn_number, wo);
        success = r.success;
        roll_info = log::RollInfo{seed, r};
    }

    // [VALIDATOR] -- run the gauntlet against the branch the roll (or its
    // absence) actually selected.
    const std::vector<Effect>& selected_effects = success ? wo.effects_on_success : wo.effects_on_failure;
    const std::string& selected_narration = success ? wo.narration_on_success : wo.narration_on_failure;

    auto gauntlet_result = gauntlet::run_gauntlet(world, selected_effects);

    if (auto* applied = std::get_if<std::vector<Effect>>(&gauntlet_result)) {
        // [APPLY] + [LOG]
        World world_after = applier::apply(world, *applied);
        log::Outcome outcome = log::Applied{roll_info, *applied,    selected_narration,
                                            world,     world_after, applier::vital_transitions(world, *applied)};
        log::append_to_file(log_path, log::LogEntry{turn_number, wo, outcome});
        return {world_after, outcome};
    }

    // [LOG] -- turn rolled (maybe) but never reached [APPLY].
    const auto& f = std::get<gauntlet::Failure>(gauntlet_result);
    log::Outcome outcome = log::GauntletRejected{roll_info, f.effect_index, gauntlet::stage_name(f.stage), f.reason,
                                                 f.conflicting_with_index};
    log::append_to_file(log_path, log::LogEntry{turn_number, wo, outcome});
    return {world, outcome};
}

}  // namespace gm::turn
