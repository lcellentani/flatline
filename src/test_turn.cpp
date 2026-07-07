// Verification harness for gm_turn.hpp -- Phase 1, Task 1.1 acceptance
// (gm-multiturn-loop.md). Exercises process_turn() by independently
// re-deriving the expected result from the same lower-level calls it
// wraps (validate_modifiers / resolve_turn / run_gauntlet / apply), so this
// test catches process_turn diverging from correct behavior, not just a
// compile error:
//   - bribe_the_guard -> Applied, cross-checked against a directly-run
//     pipeline
//   - the hallucination worked example -> GauntletRejected at the same
//     stage/index a directly-run gauntlet call reports, World unchanged
//   - exactly one log line appended per call, across all three outcome kinds
//
// Build: g++ -std=c++20 -I. -I<path-to-nlohmann> -Wall -Wextra test_turn.cpp -o test_turn

#include "gm_applier.hpp"
#include "gm_effects.hpp"
#include "gm_gauntlet.hpp"
#include "gm_json.hpp"
#include "gm_log.hpp"
#include "gm_roll.hpp"
#include "gm_turn.hpp"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace gm;

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) { std::cout << "PASS  " << name << "\n"; ++g_pass; }
    else      { std::cout << "FAIL  " << name << "\n"; ++g_fail; }
}

static std::string slurp(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main() {
    const char* kScratchLog = "test_turn_log.jsonl";
    std::remove(kScratchLog);

    World room = parse_world(slurp("room_guardroom.json"));

    // --- 1. bribe -> Applied, matches an independently-run pipeline --------
    {
        WorkOrder wo = parse_work_order(slurp("workorder_bribe.json"));
        auto [world_after, outcome] = turn::process_turn(room, /*turn_number=*/1, wo, kScratchLog);

        auto* applied = std::get_if<log::Applied>(&outcome);
        check(applied != nullptr, "bribe: outcome is Applied");
        if (applied) {
            uint64_t seed = roll::seed_for_turn(1, wo);
            roll::RollResult r = roll::resolve_turn(1, wo);
            check(applied->roll.has_value() && applied->roll->seed == seed && applied->roll->result.die == r.die,
                  "bribe: roll seed/die match an independently-recomputed roll");

            const auto& selected = r.success ? wo.effects_on_success : wo.effects_on_failure;
            auto gauntlet_result = gauntlet::run_gauntlet(room, selected);
            auto* expected_effects = std::get_if<std::vector<Effect>>(&gauntlet_result);
            check(expected_effects != nullptr, "bribe: gauntlet independently confirms survival");
            if (expected_effects) {
                check(json(applied->applied_effects) == json(*expected_effects),
                      "bribe: applied_effects match an independently-run gauntlet");
                World expected_world_after = applier::apply(room, *expected_effects);
                check(json(applied->world_after) == json(expected_world_after),
                      "bribe: world_after matches an independently-run apply()");
                check(json(world_after) == json(applied->world_after),
                      "bribe: process_turn's returned World == Outcome's world_after");
            }
            check(json(applied->world_before) == json(room), "bribe: world_before is the untouched input World");
        }
    }

    // --- 2. hallucinated -> GauntletRejected at the same stage/index, World unchanged ---
    {
        WorkOrder wo = parse_work_order(slurp("workorder_hallucinated.json"));
        auto [world_after, outcome] = turn::process_turn(room, /*turn_number=*/2, wo, kScratchLog);

        auto* rejected = std::get_if<log::GauntletRejected>(&outcome);
        check(rejected != nullptr, "hallucinated: outcome is GauntletRejected");
        if (rejected) {
            auto gauntlet_result = gauntlet::run_gauntlet(room, wo.effects_on_success);
            auto* expected_failure = std::get_if<gauntlet::Failure>(&gauntlet_result);
            check(expected_failure != nullptr, "hallucinated: an independently-run gauntlet also rejects");
            if (expected_failure) {
                check(rejected->effect_index == expected_failure->effect_index,
                      "hallucinated: effect_index matches");
                check(rejected->stage == gauntlet::stage_name(expected_failure->stage),
                      "hallucinated: stage matches");
                check(rejected->stage == "ids_exist", "hallucinated: rejection stage is specifically ids_exist");
                check(rejected->reason == expected_failure->reason, "hallucinated: reason string matches");
            }
        }
        check(json(world_after) == json(room), "hallucinated: returned World is unchanged on rejection");
    }

    // --- 3. exactly one log line appended per call, for every outcome kind ---
    {
        std::remove(kScratchLog);
        WorkOrder bribe = parse_work_order(slurp("workorder_bribe.json"));
        WorkOrder halluc = parse_work_order(slurp("workorder_hallucinated.json"));

        turn::process_turn(room, 1, bribe, kScratchLog);
        check(log::read_log(kScratchLog).size() == 1, "log: exactly one entry after first call (Applied)");

        turn::process_turn(room, 2, halluc, kScratchLog);
        check(log::read_log(kScratchLog).size() == 2, "log: exactly one MORE entry after second call (GauntletRejected)");

        WorkOrder over_budget = bribe;
        over_budget.modifiers = {{"too_much", 100}};
        turn::process_turn(room, 3, over_budget, kScratchLog);
        auto after_three = log::read_log(kScratchLog);
        check(after_three.size() == 3, "log: exactly one MORE entry after third call (ModifierBudgetRejected)");
        check(std::holds_alternative<log::ModifierBudgetRejected>(after_three.back().outcome),
              "log: third entry's outcome is ModifierBudgetRejected");

        std::remove(kScratchLog);
    }

    std::cout << "\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
