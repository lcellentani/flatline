// Verification harness for gm_log.hpp -- not a project artifact, just the
// compile+behavior gate for session log 8's change. Exercises:
//   - JSON round-trip for all four Outcome kinds (ModifierBudgetRejected,
//     GauntletRejected, Applied, Observed), including the rolled/no-roll
//     variants of each
//   - Observed (woz-gui-orchestrator-flow, Task 1.1): hand-built like
//     ModifierBudgetRejected -- a Tier-0 beat never touches the
//     gauntlet/roll/applier, so it's never constructed via the pipeline
//   - Outcome::from_json: an unrecognized "status" tag still throws
//     std::runtime_error -- regression proving the new "observed" branch
//     doesn't loosen this check for any other tag
//   - GauntletRejected.conflicting_with_index (session log 11): absent on
//     an ordinary rejection, present and round-trips as a real value on a
//     batch_conflict rejection, and a pre-existing log line written before
//     the field existed still parses with it defaulting to absent
//   - replay_matches(): the actual replay guarantee -- re-running apply()
//     on a logged Applied entry's own (world_before, applied_effects)
//     reproduces its own world_after
//   - replay_matches(): catches a tampered/inconsistent entry (proves the
//     check isn't a tautology)
//   - append_to_file / read_log: JSONL round-trip across multiple entries,
//     including that append is genuinely additive (doesn't clobber prior
//     lines)
//   - player_input: a non-empty value round-trips verbatim and stays
//     distinct from work_order.interpretation, an uncaptured turn writes
//     "" rather than omitting the key, and a log line predating the field
//     still parses with it defaulting to empty
//   - Applied.vital_transitions: JSON round-trip for a non-empty case (the
//     ADL-reopened VitalTransition serializer, gm_log.hpp, same pattern
//     already used for RollResult and already burned once on this exact
//     failure mode), and confirms an empty vital_transitions list still
//     serializes as [] rather than being omitted
//
// Build: g++ -std=c++20 -I. -I<path-to-nlohmann> -Wall -Wextra test_log.cpp -o test_log

#include <cstdio>
#include <iostream>

#include "gm_applier.hpp"
#include "gm_gauntlet.hpp"
#include "gm_json.hpp"
#include "gm_log.hpp"
#include "gm_roll.hpp"

using namespace gm;

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        std::cout << "PASS  " << name << "\n";
        ++g_pass;
    } else {
        std::cout << "FAIL  " << name << "\n";
        ++g_fail;
    }
}

// Builds a real Applied outcome by actually running the pipeline on
// bribe_the_guard, the same fixture test_roll.cpp and test_applier.cpp
// already use -- not a hand-built fake, so this test also incidentally
// exercises the full [MODIFIER BUDGET] -> [ROLL] -> [GAUNTLET] -> [APPLY]
// chain end to end.
static log::LogEntry build_applied_entry(const World& world, int64_t turn_number) {
    WorkOrder wo = examples::bribe_the_guard();

    auto budget_failure = gauntlet::validate_modifiers(wo.modifiers);
    if (budget_failure) throw std::runtime_error("fixture should pass the modifier budget");

    uint64_t seed = roll::seed_for_turn(turn_number, wo);
    roll::RollResult r = roll::resolve_turn(turn_number, wo);

    const std::vector<Effect>& selected = r.success ? wo.effects_on_success : wo.effects_on_failure;
    const std::string& narration = r.success ? wo.narration_on_success : wo.narration_on_failure;

    auto gauntlet_result = gauntlet::run_gauntlet(world, selected);
    auto* applied_effects = std::get_if<std::vector<Effect>>(&gauntlet_result);
    if (!applied_effects) throw std::runtime_error("fixture should pass the gauntlet");

    World world_after = applier::apply(world, *applied_effects);

    log::Applied applied{
        .roll = log::RollInfo{seed, r},
        .applied_effects = *applied_effects,
        .narration = narration,
        .world_before = world,
        .world_after = world_after,
    };
    return log::LogEntry{turn_number, wo, applied};
}

int main() {
    World world = parse_world(R"({
        "room": {"id": "guardroom", "exits": {}},
        "objects": [],
        "entities": [{"id": "guard_01", "hp": 10, "disposition": "neutral", "holds": ["key_north"], "persona": ""}],
        "player": {"id": "player", "hp": 10, "gold": 20, "inventory": []}
    })");

    // --- 1. Applied: JSON round-trip preserves every field -----------------
    {
        // Find a turn_number that actually succeeds so world_before !=
        // world_after (exercises more of the payload than a no-op would).
        int64_t turn = 0;
        log::LogEntry entry = build_applied_entry(world, turn);
        while (!std::get<log::Applied>(entry.outcome).roll->result.success && turn < 100)
            entry = build_applied_entry(world, ++turn);

        std::string dumped = json(entry).dump();
        log::LogEntry parsed = json::parse(dumped).get<log::LogEntry>();

        check(parsed.turn_number == entry.turn_number, "Applied round-trip: turn_number preserved");
        check(json(parsed.work_order) == json(entry.work_order), "Applied round-trip: work_order preserved");

        auto& a1 = std::get<log::Applied>(entry.outcome);
        auto& a2 = std::get<log::Applied>(parsed.outcome);
        check(a1.roll.has_value() && a2.roll.has_value(), "Applied round-trip: roll present on both sides");
        check(a1.roll->seed == a2.roll->seed && a1.roll->result.die == a2.roll->result.die,
              "Applied round-trip: seed and die preserved");
        check(json(a1.applied_effects) == json(a2.applied_effects), "Applied round-trip: applied_effects preserved");
        check(a1.narration == a2.narration, "Applied round-trip: narration preserved");
        check(json(a1.world_before) == json(a2.world_before), "Applied round-trip: world_before preserved");
        check(json(a1.world_after) == json(a2.world_after), "Applied round-trip: world_after preserved");
    }

    // --- 2. replay_matches(): the actual replay guarantee ------------------
    {
        int64_t turn = 0;
        log::LogEntry entry = build_applied_entry(world, turn);
        // A no-op turn (failure branch with only a disposition change) still
        // proves the property; no need to search for a success here.
        auto& applied = std::get<log::Applied>(entry.outcome);
        check(log::replay_matches(applied),
              "replay_matches: re-running apply(world_before, applied_effects) reproduces world_after");
    }

    // --- 3. replay_matches(): catches a genuinely broken record -------------
    //        (proves this isn't a tautology -- it can actually fail)
    {
        int64_t turn = 0;
        log::LogEntry entry = build_applied_entry(world, turn);
        auto applied = std::get<log::Applied>(entry.outcome);
        applied.world_after.player.gold += 999;  // corrupt the recorded outcome
        check(!log::replay_matches(applied),
              "replay_matches: tampered world_after is correctly rejected, not silently accepted");
    }

    // --- 4. ModifierBudgetRejected: JSON round-trip, no roll field ever ----
    {
        log::ModifierBudgetRejected rejected{7, 5, "modifier magnitude sum 7 exceeds budget 5"};
        WorkOrder wo = examples::bribe_the_guard();
        log::LogEntry entry{3, wo, rejected};

        json j = entry;
        check(j.at("status") == "modifier_budget_rejected", "ModifierBudgetRejected: status tag correct");
        check(!j.contains("roll") && !j.contains("world_before") && !j.contains("applied_effects"),
              "ModifierBudgetRejected: no roll/world/effects fields leak into the JSON at all");

        log::LogEntry parsed = j.get<log::LogEntry>();
        auto& r = std::get<log::ModifierBudgetRejected>(parsed.outcome);
        check(r.magnitude_sum == 7 && r.budget == 5 && r.reason == rejected.reason,
              "ModifierBudgetRejected: round-trip preserves magnitude_sum/budget/reason");
    }

    // --- 5. GauntletRejected: with a roll, and without one ------------------
    {
        WorkOrder wo = examples::hallucinated_item();

        // 5a. with a roll (turn rolled, then failed the gauntlet on the branch selected)
        log::GauntletRejected with_roll{log::RollInfo{12345ULL, roll::RollResult{15, 2, 17, true}}, 0, "ids_exist",
                                        "transfer_item.item 'silver_key' does not exist"};
        json j1 = log::LogEntry{4, wo, with_roll};
        check(j1.contains("roll"), "GauntletRejected: roll field present when the turn actually rolled");
        auto parsed1 = j1.get<log::LogEntry>();
        auto& g1 = std::get<log::GauntletRejected>(parsed1.outcome);
        check(g1.roll.has_value() && g1.roll->seed == 12345ULL, "GauntletRejected: roll round-trips when present");
        check(g1.effect_index == 0 && g1.stage == "ids_exist", "GauntletRejected: effect_index/stage preserved");

        check(!g1.conflicting_with_index.has_value(),
              "GauntletRejected: conflicting_with_index absent on an ids_exist rejection");

        // 5b. no roll (no-roll turn, still failed the gauntlet)
        log::GauntletRejected no_roll{std::nullopt, 0, "ids_exist", "same failure, no-roll turn"};
        json j2 = log::LogEntry{5, wo, no_roll};
        check(!j2.contains("roll"), "GauntletRejected: roll field absent (not null) on a no-roll turn");
        auto parsed2 = j2.get<log::LogEntry>();
        auto& g2 = std::get<log::GauntletRejected>(parsed2.outcome);
        check(!g2.roll.has_value(), "GauntletRejected: absent roll round-trips as absent, not a phantom value");

        // 5c. a same-batch conflict (session log 11) -- conflicting_with_index
        // present this time, and must round-trip as a real value, not omitted.
        log::GauntletRejected conflict{std::nullopt, 1, "batch_conflict",
                                       "transfer_item.from 'guard_01' does not hold item 'key_north'", 0};
        json j3 = log::LogEntry{6, wo, conflict};
        check(j3.contains("conflicting_with_index"),
              "GauntletRejected: conflicting_with_index present in JSON for a batch_conflict rejection");
        auto parsed3 = j3.get<log::LogEntry>();
        auto& g3 = std::get<log::GauntletRejected>(parsed3.outcome);
        check(g3.stage == "batch_conflict" && g3.conflicting_with_index.has_value() && *g3.conflicting_with_index == 0,
              "GauntletRejected: conflicting_with_index round-trips as a real value, not omitted");

        // 5d. a log line written before this field existed still parses,
        // with the field defaulting to absent rather than throwing.
        json j4 = j2;  // j2 (5b) predates the field by construction
        auto parsed4 = j4.get<log::LogEntry>();
        auto& g4 = std::get<log::GauntletRejected>(parsed4.outcome);
        check(!g4.conflicting_with_index.has_value(),
              "GauntletRejected: pre-existing log line without the field parses, defaults to absent");
    }

    // --- 6. Applied, no-roll turn (burn_the_door: modifiers empty) ---------
    {
        WorkOrder wo = examples::burn_the_door();
        World w2 = world;
        w2.objects.push_back(ObjectState{{"door_north"}, {{"locked", true}}});
        w2.player.inventory.push_back({"oil_flask"});

        auto gauntlet_result = gauntlet::run_gauntlet(w2, wo.effects_on_success);
        auto* eff = std::get_if<std::vector<Effect>>(&gauntlet_result);
        check(eff != nullptr, "no-roll fixture: burn_the_door passes the gauntlet (sanity)");
        World after = applier::apply(w2, *eff);

        log::Applied applied{std::nullopt, *eff, wo.narration_on_success, w2, after};
        log::LogEntry entry{6, wo, applied};

        json j = entry;
        check(!j.contains("roll"), "Applied no-roll: roll field absent in JSON (no phantom seed/die logged)");
        auto parsed = j.get<log::LogEntry>();
        auto& a = std::get<log::Applied>(parsed.outcome);
        check(!a.roll.has_value(), "Applied no-roll: round-trips with roll absent");
        check(log::replay_matches(a), "Applied no-roll: replay still holds with no roll involved");
    }

    // --- 7. append_to_file / read_log: JSONL round-trip, genuinely additive -
    {
        const std::string path = "test_log_output.jsonl";
        std::remove(path.c_str());

        log::LogEntry e1{1, examples::burn_the_door(), log::ModifierBudgetRejected{7, 5, "over budget"}};
        log::LogEntry e2 = build_applied_entry(world, 2);

        log::append_to_file(path, e1);
        log::append_to_file(path, e2);

        auto entries = log::read_log(path);
        check(entries.size() == 2, "append_to_file/read_log: two appended entries -> two entries read back");
        check(entries[0].turn_number == 1 && entries[1].turn_number == 2, "append_to_file/read_log: order preserved");
        check(std::holds_alternative<log::ModifierBudgetRejected>(entries[0].outcome),
              "append_to_file/read_log: first entry's outcome kind preserved");
        check(std::holds_alternative<log::Applied>(entries[1].outcome),
              "append_to_file/read_log: second entry's outcome kind preserved");

        // Append a third and confirm the first two survive untouched --
        // proves this is append, not rewrite.
        log::append_to_file(
            path, log::LogEntry{3, examples::burn_the_door(), log::ModifierBudgetRejected{6, 5, "still over"}});
        auto entries2 = log::read_log(path);
        check(entries2.size() == 3 && entries2[0].turn_number == 1 && entries2[1].turn_number == 2,
              "append_to_file: appending a third entry leaves the first two intact (genuinely additive)");

        std::remove(path.c_str());
    }

    // --- 8. Applied.vital_transitions: JSON round-trip, non-empty case -----
    //        (ADL-reopened VitalTransition serializer -- the exact failure
    //        mode already caught once for RollResult, session log 8)
    {
        World w2 = world;  // this file's own minimal fixture (guard_01 hp=10)
        std::vector<Effect> batch = {ModifyResource{{"guard_01"}, ModifyResource::Resource::Hp, -999}};

        auto gauntlet_result = gauntlet::run_gauntlet(w2, batch);
        auto* eff = std::get_if<std::vector<Effect>>(&gauntlet_result);
        check(eff != nullptr, "vital_transitions fixture: -999 hp passes the gauntlet (sanity)");

        World after = applier::apply(w2, *eff);
        auto transitions = applier::vital_transitions(w2, *eff);
        check(transitions.size() == 1 && transitions[0].to == VitalState::Dead,
              "vital_transitions fixture: guard_01 Alive -> Dead recorded (sanity)");

        log::Applied applied{std::nullopt, *eff, "The guard collapses.", w2, after, transitions};
        log::LogEntry entry{7, examples::burn_the_door(), applied};

        json j = entry;
        check(j["vital_transitions"].is_array() && j["vital_transitions"].size() == 1,
              "Applied JSON: vital_transitions serialized as a one-element array");

        log::LogEntry parsed = j.get<log::LogEntry>();
        auto& a = std::get<log::Applied>(parsed.outcome);
        check(a.vital_transitions.size() == 1, "Applied round-trip: vital_transitions count preserved");
        check(a.vital_transitions[0].entity == EntityId{"guard_01"} &&
                  a.vital_transitions[0].from == VitalState::Alive && a.vital_transitions[0].to == VitalState::Dead &&
                  a.vital_transitions[0].overkill == transitions[0].overkill,
              "Applied round-trip: entity/from/to/overkill all preserved (ADL-reopened serializer correct)");
    }

    // --- 9. Applied.vital_transitions: present as [] on an ordinary turn,
    //        not omitted (unlike `roll`, which IS omitted when absent) -----
    {
        log::LogEntry entry = build_applied_entry(world, 8);  // bribe fixture, no hp effects
        json j = entry;
        check(j.contains("vital_transitions") && j["vital_transitions"].is_array() && j["vital_transitions"].empty(),
              "Applied JSON: vital_transitions present as an empty array, not omitted, on a non-combat turn");
    }

    // --- 10. player_input: round-trips, and is tolerant of omission --------
    //         The benchmark's real input is (pre-state, the player's raw
    //         words); interpretation is the GM's paraphrase, so this field
    //         is the one that must survive the log verbatim.
    {
        // 10a. a non-empty value survives dump -> parse unchanged
        log::LogEntry entry = build_applied_entry(world, 9);
        entry.player_input = "I slip the guard twenty gold and ask about the north door";

        std::string dumped = json(entry).dump();
        log::LogEntry parsed = json::parse(dumped).get<log::LogEntry>();
        check(parsed.player_input == entry.player_input,
              "player_input: non-empty value round-trips through dump/parse verbatim");
        check(parsed.player_input != parsed.work_order.interpretation,
              "player_input: stays distinct from work_order.interpretation (not collapsed into it)");

        // 10b. present as "" rather than omitted on a turn with no raw input
        // -- the same present-as-empty contract vital_transitions has ([] not
        // omitted), so a reader can tell "captured, empty" from "predates the
        // field".
        log::LogEntry no_input = build_applied_entry(world, 10);
        json j_empty = no_input;
        check(j_empty.contains("player_input") && j_empty["player_input"].is_string() &&
                  j_empty["player_input"].get<std::string>().empty(),
              "player_input: present as an empty string, not omitted, when nothing was captured");
        check(json::parse(j_empty.dump()).get<log::LogEntry>().player_input.empty(),
              "player_input: an explicitly-empty value round-trips as empty");

        // 10c. a log line written before this field existed still parses,
        // defaulting to empty rather than throwing -- same tolerance already
        // proven for conflicting_with_index in case 5d.
        json j_old = j_empty;
        j_old.erase("player_input");
        log::LogEntry parsed_old = j_old.get<log::LogEntry>();
        check(parsed_old.player_input.empty(),
              "player_input: pre-existing log line without the field parses, defaults to empty");
        check(
            parsed_old.turn_number == no_input.turn_number && std::holds_alternative<log::Applied>(parsed_old.outcome),
            "player_input: the rest of a pre-existing entry is unaffected by the missing field");
    }

    // --- 11. Observed: JSON round-trip, hand-built (no pipeline run) -------
    //         Mirrors case 4's shape: a Tier-0 beat never touches the
    //         gauntlet/roll/applier, so it's constructed directly rather
    //         than via build_applied_entry().
    {
        log::Observed observed{"examine", "guard_01", "The guard eyes you warily, hand on his hilt."};
        WorkOrder wo = examples::bribe_the_guard();
        log::LogEntry entry{11, wo, observed};

        json j = entry;
        check(j.at("status") == "observed", "Observed: status tag correct");
        check(!j.contains("roll") && !j.contains("world_before") && !j.contains("applied_effects"),
              "Observed: no roll/world/effects fields leak into the JSON at all");

        log::LogEntry parsed = j.get<log::LogEntry>();
        auto& o = std::get<log::Observed>(parsed.outcome);
        check(o.kind == observed.kind && o.target == observed.target && o.narration == observed.narration,
              "Observed round-trip: kind/target/narration preserved");
    }

    // --- 12. Outcome::from_json: an unrecognized status still throws -------
    //         (regression -- adding the "observed" branch must not loosen
    //         this check for any other/garbage tag)
    {
        json j = json{{"status", "not_a_real_status"}};
        bool threw = false;
        try {
            log::Outcome bad = j.get<log::Outcome>();
            (void)bad;
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw, "Outcome::from_json: unrecognized status still throws std::runtime_error");
    }

    std::cout << "\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
