// Verification harness for gm_roll.hpp + gm_gauntlet.hpp's validate_modifiers
// -- not a project artifact, just the compile+behavior gate for session
// log 7's change. Exercises:
//   - fnv1a_64: deterministic, distinguishes distinct inputs
//   - resolve(): deterministic given (seed, modifier_sum); die always 1..20;
//     total == die + modifier_sum
//   - resolve_turn(): the actual replay guarantee -- two independently
//     constructed, content-identical WorkOrders at the same turn_number
//     produce an identical roll
//   - resolve_turn(): varying turn_number actually varies the outcome
//     (catches a seed that silently ignores turn_number)
//   - modifier_budget_legal() / gauntlet::validate_modifiers(): the
//     locked kModifierBudget=5 at kDifficultyClass=15, both directions
//     (positive-heavy and negative-heavy), and the bribe_the_guard example
//     sitting exactly at the cap
//
// Build: g++ -std=c++20 -I. -Wall -Wextra test_roll.cpp -o test_roll

#include "gm_roll.hpp"
#include "gm_gauntlet.hpp"
#include "gm_json.hpp"
#include <cassert>
#include <iostream>
#include <set>

using namespace gm;

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) { std::cout << "PASS  " << name << "\n"; ++g_pass; }
    else      { std::cout << "FAIL  " << name << "\n"; ++g_fail; }
}

int main() {
    // --- 1. fnv1a_64: deterministic, distinguishes distinct inputs --------
    {
        check(roll::fnv1a_64("hello") == roll::fnv1a_64("hello"),
              "fnv1a_64: same input -> same hash");
        check(roll::fnv1a_64("hello") != roll::fnv1a_64("world"),
              "fnv1a_64: different input -> different hash");
    }

    // --- 2. resolve(): pure, deterministic, die in range -------------------
    {
        auto r1 = roll::resolve(12345ULL, 5);
        auto r2 = roll::resolve(12345ULL, 5);
        check(r1.die == r2.die && r1.total == r2.total && r1.success == r2.success,
              "resolve: same (seed, modifier_sum) -> identical RollResult");
        check(r1.die >= 1 && r1.die <= 20, "resolve: die always in [1,20]");
        check(r1.total == r1.die + r1.modifier_sum, "resolve: total == die + modifier_sum");
        check(r1.success == (r1.total >= rules::kDifficultyClass),
              "resolve: success == (total >= kDifficultyClass)");
    }

    // --- 3. resolve(): different seeds produce different dice (spot check,
    //        not a distribution test) -- catches a seed that's ignored -----
    {
        std::set<int32_t> dice_seen;
        for (uint64_t s = 0; s < 25; ++s) dice_seen.insert(roll::resolve(s, 0).die);
        check(dice_seen.size() > 1, "resolve: varying seed produces varying die results");
    }

    // --- 4. resolve_turn(): the actual replay guarantee --------------------
    //        Two SEPARATELY CONSTRUCTED WorkOrders with identical content
    //        (not the same object) must produce an identical roll -- this
    //        is the property a replay-from-log depends on.
    {
        WorkOrder wo1 = examples::bribe_the_guard();
        WorkOrder wo2 = examples::bribe_the_guard();
        auto r1 = roll::resolve_turn(7, wo1);
        auto r2 = roll::resolve_turn(7, wo2);
        check(r1.die == r2.die && r1.modifier_sum == r2.modifier_sum && r1.success == r2.success,
              "resolve_turn: identical turn_number + content -> identical roll (replay guarantee)");
    }

    // --- 5. resolve_turn(): turn_number actually participates in the seed --
    {
        WorkOrder wo = examples::bribe_the_guard();
        std::set<int32_t> dice_seen;
        for (int64_t t = 0; t < 15; ++t) dice_seen.insert(roll::resolve_turn(t, wo).die);
        check(dice_seen.size() > 1,
              "resolve_turn: varying turn_number (same content) varies the die (seed isn't ignoring it)");
    }

    // --- 6. resolve_turn(): content actually participates in the seed ------
    {
        WorkOrder wo_bribe = examples::bribe_the_guard();
        WorkOrder wo_burn  = examples::burn_the_door();
        check(roll::seed_for_turn(1, wo_bribe) != roll::seed_for_turn(1, wo_burn),
              "resolve_turn: different WorkOrder content (same turn_number) yields a different seed");
    }

    // --- 7. modifier_budget_legal(): bribe_the_guard sits exactly at the cap
    {
        auto wo = examples::bribe_the_guard();
        int64_t magnitude = 0;
        for (const auto& m : wo.modifiers) magnitude += std::abs(static_cast<int64_t>(m.value));
        check(magnitude == 5, "bribe_the_guard: modifier magnitude sum is exactly 5 (sanity on the fixture)");
        check(rules::modifier_budget_legal(wo.modifiers),
              "modifier_budget_legal: bribe_the_guard (magnitude 5) legal at budget 5");
        check(!gauntlet::validate_modifiers(wo.modifiers).has_value(),
              "gauntlet::validate_modifiers: bribe_the_guard passes");
    }

    // --- 8. modifier_budget_legal(): over budget on the positive side ------
    {
        std::vector<Modifier> over = {{"a", 4}, {"b", 3}};  // magnitude 7 > budget 5
        check(!rules::modifier_budget_legal(over),
              "modifier_budget_legal: magnitude 7 (positive-heavy) rejected");
        auto failure = gauntlet::validate_modifiers(over);
        check(failure.has_value() && failure->magnitude_sum == 7 && failure->budget == 5,
              "gauntlet::validate_modifiers: over-budget batch reports correct magnitude_sum and budget");
    }

    // --- 9. modifier_budget_legal(): over budget on the negative side ------
    //        (proves the cap is on MAGNITUDE, not net sum -- locked decision)
    {
        std::vector<Modifier> negative_heavy = {{"a", -3}, {"b", -3}};  // net -6, magnitude 6
        check(!rules::modifier_budget_legal(negative_heavy),
              "modifier_budget_legal: negative-heavy stack (magnitude 6) also rejected, not just net-sum-bounded");
    }

    // --- 10. modifier_budget_legal(): mixed batch spends from one pool -----
    {
        std::vector<Modifier> mixed = {{"a", 3}, {"b", -2}};  // magnitude 5, net +1
        check(rules::modifier_budget_legal(mixed),
              "modifier_budget_legal: mixed +3/-2 batch (magnitude 5) legal -- one shared pool");
    }

    // --- 11. modifier_budget_legal(): empty modifiers (no-roll turn) -------
    {
        auto wo = examples::burn_the_door();
        check(rules::modifier_budget_legal(wo.modifiers),
              "modifier_budget_legal: empty modifiers (no-roll turn) trivially legal");
    }

    // --- 12. Boundary check: exactly at budget (5) legal, one over (6) not -
    {
        std::vector<Modifier> at_cap = {{"a", 5}};
        std::vector<Modifier> over_cap = {{"a", 6}};
        check(rules::modifier_budget_legal(at_cap), "modifier_budget_legal: exactly at cap (5) legal (<=, not <)");
        check(!rules::modifier_budget_legal(over_cap), "modifier_budget_legal: one over cap (6) rejected");
    }

    std::cout << "\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
