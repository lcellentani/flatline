// Verification harness for gm_gauntlet.hpp — not a project artifact, just
// the compile+behavior gate for session log 5's change. Exercises:
//   - all three worked examples from gm_effects.hpp (bribe/burn/hallucinated)
//   - both sentinel carve-outs (VOID_ENTITY, UNARMED_ITEM)
//   - damage clamping to the cap table
//   - the two newly-surfaced eligibility gaps (disposition_eligible on
//     player, resource_eligible for gold on a non-player entity)
//   - a genuine damage-cap table gap, distinct from a hallucination
//   - vital-state preconditions (spec §5, hp ceiling and alive/dead
//     state): modify_disposition rejected on Unconscious AND Dead;
//     damage/modify_resource(hp) rejected on Dead only; transfer_item
//     deliberately still legal against a Dead entity (looting)
//   - same-batch effect conflicts (spec §5, session log 11): transfer_item
//     duplication and gold double-spend now caught as Stage::BatchConflict
//     with conflicting_with_index set; an ordinary never-legal precondition
//     failure stays Stage::Preconditions with the field absent; an item
//     consumed earlier in the batch and re-referenced is a batch conflict,
//     not a hallucination; modify_disposition compounds correctly at the
//     enum ceiling across a batch instead of both effects clamping from the
//     same pre-turn value
//
// Build: g++ -std=c++20 -I. -Wall -Wextra test_gauntlet.cpp -o test_gauntlet

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

#include "gm_gauntlet.hpp"
#include "gm_json.hpp"

using namespace gm;
using namespace gm::gauntlet;

static std::string slurp(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static Entity* find_entity(World& w, const char* id) {
    for (auto& e : w.entities)
        if (e.id == EntityId{id}) return &e;
    return nullptr;
}

static int g_pass = 0, g_fail = 0;

static void expect_success(const char* name, const Result& r, size_t expected_count) {
    if (auto* effects = std::get_if<std::vector<Effect>>(&r)) {
        if (effects->size() == expected_count) {
            std::cout << "PASS  " << name << " -> " << effects->size() << " effect(s) applied\n";
            ++g_pass;
        } else {
            std::cout << "FAIL  " << name << " -> wrong effect count (" << effects->size() << " != " << expected_count
                      << ")\n";
            ++g_fail;
        }
    } else {
        const auto& f = std::get<Failure>(r);
        std::cout << "FAIL  " << name << " -> unexpected rejection at effect " << f.effect_index << " ["
                  << stage_name(f.stage) << "]: " << f.reason << "\n";
        ++g_fail;
    }
}

static void expect_failure(const char* name, const Result& r, Stage expected_stage) {
    if (auto* f = std::get_if<Failure>(&r)) {
        if (f->stage == expected_stage) {
            std::cout << "PASS  " << name << " -> rejected at effect " << f->effect_index << " ["
                      << stage_name(f->stage) << "]: " << f->reason << "\n";
            ++g_pass;
        } else {
            std::cout << "FAIL  " << name << " -> rejected at wrong stage [" << stage_name(f->stage) << "], expected ["
                      << stage_name(expected_stage) << "]\n";
            ++g_fail;
        }
    } else {
        std::cout << "FAIL  " << name << " -> expected rejection, got success\n";
        ++g_fail;
    }
}

int main() {
    World world = parse_world(slurp("room_guardroom.json"));

    // --- 1. Worked examples, as-authored -----------------------------------
    expect_success("bribe_the_guard: success branch",
                   run_gauntlet(world, examples::bribe_the_guard().effects_on_success), 3);
    expect_success("bribe_the_guard: failure branch",
                   run_gauntlet(world, examples::bribe_the_guard().effects_on_failure), 1);
    expect_success("burn_the_door: success branch (VOID_ENTITY carve-out)",
                   run_gauntlet(world, examples::burn_the_door().effects_on_success), 2);
    expect_failure("hallucinated_item: rejected, not silently accepted",
                   run_gauntlet(world, examples::hallucinated_item().effects_on_success), Stage::IdsExist);

    // --- 2. Fail-fast: hallucination in effect 2 must report index 1, not 0 -
    {
        std::vector<Effect> batch = {
            ModifyDisposition{{"guard_01"}, +1},                             // legal
            TransferItem{{"silver_key"}, {"guard_01"}, {"player"}},          // hallucinated
            ModifyResource{{"player"}, ModifyResource::Resource::Gold, -1},  // never reached
        };
        auto r = run_gauntlet(world, batch);
        const auto* f = std::get_if<Failure>(&r);
        assert(f && f->effect_index == 1 && f->stage == Stage::IdsExist);
        std::cout << "PASS  fail-fast stops at first bad effect (index 1, not 0 or 2)\n";
        ++g_pass;
    }

    // --- 3. UNARMED_ITEM sentinel: no holds() check, clamps to its own cap -
    {
        std::vector<Effect> batch = {Damage{{"player"}, {"guard_01"}, UNARMED_ITEM, 999}};
        auto r = run_gauntlet(world, batch);
        auto* effects = std::get_if<std::vector<Effect>>(&r);
        assert(effects && effects->size() == 1);
        auto amount = std::get<Damage>((*effects)[0]).amount;
        assert(amount == 1);  // kDamageCap["__unarmed__"] == 1
        std::cout << "PASS  UNARMED_ITEM: no holds() check needed, clamped 999 -> " << amount << "\n";
        ++g_pass;
    }

    // --- 4. Damage clamps to the cap table, doesn't just pass amount through
    {
        std::vector<Effect> batch = {Damage{{"player"}, {"guard_01"}, {"dagger"}, 999}};
        auto r = run_gauntlet(world, batch);
        auto* effects = std::get_if<std::vector<Effect>>(&r);
        assert(effects && effects->size() == 1);
        auto amount = std::get<Damage>((*effects)[0]).amount;
        assert(amount == 4);  // kDamageCap["dagger"] == 4
        std::cout << "PASS  dagger damage clamped 999 -> " << amount << "\n";
        ++g_pass;
    }

    // --- 5. disposition_eligible: player has no disposition field ----------
    {
        std::vector<Effect> batch = {ModifyDisposition{{"player"}, +1}};
        expect_failure("modify_disposition(player, ...) rejected, not silently no-op'd", run_gauntlet(world, batch),
                       Stage::Preconditions);
    }

    // --- 6. resource_eligible: gold doesn't exist on an Entity --------------
    {
        std::vector<Effect> batch = {ModifyResource{{"guard_01"}, ModifyResource::Resource::Gold, -1}};
        expect_failure("modify_resource(guard_01, gold, ...) rejected — gold is player-only",
                       run_gauntlet(world, batch), Stage::Preconditions);
    }

    // --- 7. gold_spend_legal: still enforced through the new eligibility gate
    {
        std::vector<Effect> batch = {ModifyResource{{"player"}, ModifyResource::Resource::Gold, -999}};
        expect_failure("modify_resource(player, gold, -999) rejected — would go negative", run_gauntlet(world, batch),
                       Stage::Preconditions);
    }

    // --- 8. A genuine damage-cap table gap, distinct from a hallucination --
    // Add an item that exists in state but was never given a kDamageCap row.
    {
        World w2 = world;
        w2.player.inventory.push_back(ItemId{"torch"});
        std::vector<Effect> batch = {Damage{{"player"}, {"guard_01"}, {"torch"}, 3}};
        expect_failure("damage via 'torch' (real item, no cap-table row) -> table gap, not hallucination",
                       run_gauntlet(w2, batch), Stage::MagnitudeBounds);
    }

    // --- 9. modify_disposition rejected on a Dead entity --------------------
    {
        World dead_world = world;
        Entity* guard = find_entity(dead_world, "guard_01");
        assert(guard);
        guard->vital = VitalState::Dead;
        std::vector<Effect> batch = {ModifyDisposition{{"guard_01"}, +1}};
        expect_failure("modify_disposition(Dead guard) rejected — no attitude to modify",
                       run_gauntlet(dead_world, batch), Stage::Preconditions);
    }

    // --- 10. modify_disposition rejected on an Unconscious entity too, not
    //         only Dead ------------------------------------------------------
    {
        World down_world = world;
        Entity* guard = find_entity(down_world, "guard_01");
        assert(guard);
        guard->vital = VitalState::Unconscious;
        std::vector<Effect> batch = {ModifyDisposition{{"guard_01"}, +1}};
        expect_failure("modify_disposition(Unconscious guard) rejected too, not just Dead",
                       run_gauntlet(down_world, batch), Stage::Preconditions);
    }

    // --- 11. damage rejected against a Dead target (terminal) --------------
    {
        World dead_world = world;
        Entity* guard = find_entity(dead_world, "guard_01");
        assert(guard);
        guard->vital = VitalState::Dead;
        std::vector<Effect> batch = {Damage{{"player"}, {"guard_01"}, {"dagger"}, 1}};
        expect_failure("damage against a Dead target rejected", run_gauntlet(dead_world, batch), Stage::Preconditions);
    }

    // --- 12. damage still legal against an Unconscious target (contrast --
    //         finishing blows are ordinary damage until Dead) ---------------
    {
        World down_world = world;
        Entity* guard = find_entity(down_world, "guard_01");
        assert(guard);
        guard->hp = 0;
        guard->vital = VitalState::Unconscious;
        std::vector<Effect> batch = {Damage{{"player"}, {"guard_01"}, {"dagger"}, 1}};
        expect_success("damage against an Unconscious target still legal (finishing blow)",
                       run_gauntlet(down_world, batch), 1);
    }

    // --- 13. modify_resource(hp) rejected against a Dead actor (heal can't
    //         resurrect either) ---------------------------------------------
    {
        World dead_world = world;
        Entity* guard = find_entity(dead_world, "guard_01");
        assert(guard);
        guard->vital = VitalState::Dead;
        std::vector<Effect> batch = {ModifyResource{{"guard_01"}, ModifyResource::Resource::Hp, +10}};
        expect_failure("modify_resource(hp) against a Dead actor rejected — heal can't resurrect",
                       run_gauntlet(dead_world, batch), Stage::Preconditions);
    }

    // --- 14. transfer_item still legal against a Dead entity -- looting a
    //         corpse is deliberately ungated (spec §5) ----------------------
    {
        World dead_world = world;
        Entity* guard = find_entity(dead_world, "guard_01");
        assert(guard);
        guard->vital = VitalState::Dead;
        std::vector<Effect> batch = {TransferItem{{"key_north"}, {"guard_01"}, {"player"}}};
        expect_success("transfer_item (loot) against a Dead entity still legal — deliberately ungated",
                       run_gauntlet(dead_world, batch), 1);
    }

    // --- 15. Same-batch conflict: two transfer_item effects both claim the
    //         same item from the same holder to two different destinations
    //         (session log 11, the traced bug from session log 10). Under
    //         the old static-snapshot check both independently pass; folded
    //         forward, the second sees the item already gone. -------------
    {
        std::vector<Effect> batch = {
            TransferItem{{"key_north"}, {"guard_01"}, {"player"}},
            TransferItem{{"key_north"}, {"guard_01"}, VOID_ENTITY},
        };
        auto r = run_gauntlet(world, batch);
        const auto* f = std::get_if<Failure>(&r);
        assert(f && f->effect_index == 1 && f->stage == Stage::BatchConflict);
        assert(f->conflicting_with_index.has_value() && *f->conflicting_with_index == 0);
        std::cout << "PASS  transfer_item duplication -> batch_conflict at effect 1, "
                     "conflicting with effect 0: "
                  << f->reason << "\n";
        ++g_pass;
    }

    // --- 16. Same-batch conflict: two gold spends each individually fit the
    //         15-gold balance, not both together (session log 10's second
    //         traced bug). --------------------------------------------------
    {
        std::vector<Effect> batch = {
            ModifyResource{{"player"}, ModifyResource::Resource::Gold, -10},
            ModifyResource{{"player"}, ModifyResource::Resource::Gold, -10},
        };
        auto r = run_gauntlet(world, batch);
        const auto* f = std::get_if<Failure>(&r);
        assert(f && f->effect_index == 1 && f->stage == Stage::BatchConflict);
        assert(f->conflicting_with_index.has_value() && *f->conflicting_with_index == 0);
        std::cout << "PASS  gold double-spend -> batch_conflict at effect 1, "
                     "conflicting with effect 0: "
                  << f->reason << "\n";
        ++g_pass;
    }

    // --- 17. Same-batch conflict is distinct from an ordinary precondition
    //         failure: reject-at-turn-start still reports Stage::Preconditions
    //         with conflicting_with_index absent, not misfiled as a conflict.
    {
        std::vector<Effect> batch = {ModifyResource{{"player"}, ModifyResource::Resource::Gold, -999}};
        auto r = run_gauntlet(world, batch);
        const auto* f = std::get_if<Failure>(&r);
        assert(f && f->stage == Stage::Preconditions && !f->conflicting_with_index.has_value());
        std::cout << "PASS  never-legal precondition failure stays Stage::Preconditions, "
                     "conflicting_with_index absent\n";
        ++g_pass;
    }

    // --- 18. An item consumed earlier in the same batch, then referenced
    //         again, is a batch conflict — NOT a hallucination. Exercises
    //         ids_exist staying pinned to the static pre-turn world while
    //         preconditions folds forward (session log 11 header comment). -
    {
        std::vector<Effect> batch = {
            TransferItem{{"oil_flask"}, {"player"}, VOID_ENTITY},  // consumed
            Damage{{"player"}, {"guard_01"}, {"oil_flask"}, 1},    // via already gone
        };
        auto r = run_gauntlet(world, batch);
        const auto* f = std::get_if<Failure>(&r);
        assert(f && f->effect_index == 1);
        assert(f->stage == Stage::BatchConflict);  // not IdsExist -- item was real at turn start
        assert(f->conflicting_with_index.has_value() && *f->conflicting_with_index == 0);
        std::cout << "PASS  item consumed earlier in batch -> batch_conflict, not a "
                     "hallucination: "
                  << f->reason << "\n";
        ++g_pass;
    }

    // --- 19. modify_disposition compounds correctly across a batch instead
    //         of both effects clamping from the same pre-turn value. guard_01
    //         starts Friendly(+1); two "+1" effects should compound to
    //         exactly Allied(+2), not overshoot to +3. -----------------------
    {
        World friendly_world = world;
        Entity* guard = find_entity(friendly_world, "guard_01");
        assert(guard);
        guard->disposition = disposition::Friendly;
        std::vector<Effect> batch = {
            ModifyDisposition{{"guard_01"}, +1},
            ModifyDisposition{{"guard_01"}, +1},
        };
        auto r = run_gauntlet(friendly_world, batch);
        auto* effects = std::get_if<std::vector<Effect>>(&r);
        assert(effects && effects->size() == 2);
        auto d0 = std::get<ModifyDisposition>((*effects)[0]).delta;
        auto d1 = std::get<ModifyDisposition>((*effects)[1]).delta;
        assert(d0 == 1 && d1 == 0);  // first: 1->2 (in range); second: already at ceiling, clamps to +0
        assert(guard->disposition + d0 + d1 == disposition::Allied);
        std::cout << "PASS  modify_disposition compounds across the batch: +1, then +" << d1
                  << " (ceiling correctly reached at " << disposition::Allied << ", not overshot)\n";
        ++g_pass;
    }

    std::cout << "\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
