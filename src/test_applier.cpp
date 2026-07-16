// Verification harness for gm_applier.hpp — not a project artifact, just
// the compile+behavior gate for session log 6's change. Exercises:
//   - bribe_the_guard: success branch (gold spent, key transferred, disposition +1)
//   - bribe_the_guard: failure branch (disposition -1 only, nothing else moves)
//   - burn_the_door: success branch (door unlocked, oil_flask consumed)
//   - hp floor clamp, uniformly across damage AND modify_resource(hp) (locked session log 6)
//   - set_flag: writes into World::flags (previously nowhere to land)
//   - transfer_item(from == to): legal no-op
//   - apply() is pure: the original World is never mutated
//   - hp ceiling clamp, symmetric with the floor (spec §5, hp ceiling and
//     alive/dead state, locked A2)
//   - vital state transitions: Alive -> Unconscious, Alive -> Dead direct,
//     Unconscious -> Dead, Unconscious -> Alive, Dead is terminal, player
//     reaches the same states an entity does
//   - overkill magnitude: applier::vital_transitions() captures it at
//     apply time rather than reconstructing it later
//
// Build: g++ -std=c++20 -I. -Wall -Wextra test_applier.cpp -o test_applier

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

#include "gm_applier.hpp"
#include "gm_gauntlet.hpp"
#include "gm_json.hpp"

using namespace gm;

static std::string slurp(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

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

static Entity* find_entity(World& w, const char* id) {
    for (auto& e : w.entities)
        if (e.id == EntityId{id}) return &e;
    return nullptr;
}

int main() {
    World world = parse_world(slurp("room_guardroom.json"));

    // --- 1. bribe_the_guard: success branch -------------------------------
    {
        auto result = gauntlet::run_gauntlet(world, examples::bribe_the_guard().effects_on_success);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);
        World after = applier::apply(world, *effects);

        check(after.player.gold == 0, "bribe success: gold 15 -> 0");

        bool player_has_key = std::find(after.player.inventory.begin(), after.player.inventory.end(),
                                        ItemId{"key_north"}) != after.player.inventory.end();
        check(player_has_key, "bribe success: player now holds key_north");

        Entity* guard = find_entity(after, "guard_01");
        assert(guard);
        bool guard_lost_key =
            std::find(guard->holds.begin(), guard->holds.end(), ItemId{"key_north"}) == guard->holds.end();
        check(guard_lost_key, "bribe success: guard_01 no longer holds key_north");
        check(guard->disposition == disposition::Friendly, "bribe success: disposition neutral(0) -> friendly(+1)");

        // Original world must be untouched -- proves apply() is pure, not in-place.
        check(world.player.gold == 15, "bribe success: original World.player.gold untouched (pure function)");
    }

    // --- 2. bribe_the_guard: failure branch --------------------------------
    {
        auto result = gauntlet::run_gauntlet(world, examples::bribe_the_guard().effects_on_failure);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);
        World after = applier::apply(world, *effects);

        check(after.player.gold == 15, "bribe failure: gold untouched");
        Entity* guard = find_entity(after, "guard_01");
        assert(guard);
        check(guard->disposition == disposition::Unfriendly, "bribe failure: disposition neutral(0) -> unfriendly(-1)");
    }

    // --- 3. burn_the_door: success branch -----------------------------------
    {
        auto result = gauntlet::run_gauntlet(world, examples::burn_the_door().effects_on_success);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);
        World after = applier::apply(world, *effects);

        auto door_it = std::find_if(after.objects.begin(), after.objects.end(),
                                    [](const ObjectState& o) { return o.id == ObjectId{"door_north"}; });
        assert(door_it != after.objects.end());
        check(std::get<bool>(door_it->properties.at("locked")) == false, "burn: door_north.locked -> false");

        bool flask_gone = std::find(after.player.inventory.begin(), after.player.inventory.end(),
                                    ItemId{"oil_flask"}) == after.player.inventory.end();
        check(flask_gone, "burn: oil_flask consumed (transfer to VOID_ENTITY)");
    }

    // --- 4. hp floor clamp: Damage, driven past zero by the applier, not the
    //        weapon cap (the cap already fired at gauntlet stage) -----------
    {
        std::vector<Effect> batch = {Damage{{"player"}, {"guard_01"}, {"dagger"}, 999}};
        auto result = gauntlet::run_gauntlet(world, batch);  // clamps amount 999 -> 4 (dagger's cap)
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);

        World hurt = world;
        Entity* guard = find_entity(hurt, "guard_01");
        assert(guard);
        guard->hp = 2;  // 2 - 4 would be -2 without a floor

        World after = applier::apply(hurt, *effects);
        Entity* guard_after = find_entity(after, "guard_01");
        assert(guard_after);
        check(guard_after->hp == 0, "damage: hp floors at 0, doesn't go negative (2 - 4 => 0, not -2)");
    }

    // --- 5. hp floor clamp: modify_resource(hp), symmetric with damage -----
    //        (locked session log 6: the floor is a property of hp, not of
    //        which effect touched it)
    {
        std::vector<Effect> batch = {ModifyResource{{"guard_01"}, ModifyResource::Resource::Hp, -50}};
        auto result = gauntlet::run_gauntlet(world, batch);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);

        World after = applier::apply(world, *effects);  // guard_01 starts at 12 hp
        Entity* guard = find_entity(after, "guard_01");
        assert(guard);
        check(guard->hp == 0, "modify_resource(hp): floors at 0, same as damage (locked session log 6)");
    }

    // --- 6. set_flag: writes into World::flags (previously nowhere to land) -
    {
        std::vector<Effect> batch = {SetFlag{"guard_bribed_once", true}};
        auto result = gauntlet::run_gauntlet(world, batch);  // always passes the gauntlet -- dormant there
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);

        World after = applier::apply(world, *effects);
        check(after.flags.count("guard_bribed_once") == 1, "set_flag: key present in World.flags after apply");
        check(std::get<bool>(after.flags.at("guard_bribed_once")) == true, "set_flag: value round-trips as true");
        check(world.flags.empty(), "set_flag: original World.flags untouched (pure function)");
    }

    // --- 7. transfer_item(from == to): legal no-op --------------------------
    {
        std::vector<Effect> batch = {TransferItem{{"dagger"}, {"player"}, {"player"}}};
        auto result = gauntlet::run_gauntlet(world, batch);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);

        World after = applier::apply(world, *effects);
        int count = static_cast<int>(
            std::count(after.player.inventory.begin(), after.player.inventory.end(), ItemId{"dagger"}));
        check(count == 1, "transfer_item(from==to): no-op, item count unchanged (not duplicated or dropped)");
    }

    // --- 8. hp ceiling clamp: modify_resource(hp), symmetric with the floor
    //        (locked A2) -------------------------------------------------
    {
        std::vector<Effect> batch = {ModifyResource{{"guard_01"}, ModifyResource::Resource::Hp, +50}};
        auto result = gauntlet::run_gauntlet(world, batch);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);

        World after = applier::apply(world, *effects);  // guard_01 starts at 12/12 hp
        Entity* guard = find_entity(after, "guard_01");
        assert(guard);
        check(guard->hp == 12, "modify_resource(hp): heal clamps at max_hp (12+50 => 12, not 62)");
    }

    // --- 9. Alive -> Unconscious: a single hit from low hp lands at 0, not
    //        Dead (raw -2 is above kDeathOvershoot -5) -----------------
    {
        World hurt = world;
        Entity* guard = find_entity(hurt, "guard_01");
        assert(guard);
        guard->hp = 2;

        std::vector<Effect> batch = {Damage{{"player"}, {"guard_01"}, {"dagger"}, 999}};  // clamps to 4
        auto result = gauntlet::run_gauntlet(hurt, batch);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);

        World after = applier::apply(hurt, *effects);
        Entity* guard_after = find_entity(after, "guard_01");
        assert(guard_after);
        check(guard_after->hp == 0, "Unconscious case: hp floors at 0");
        check(guard_after->vital == VitalState::Unconscious,
              "single dagger hit from low hp -> Unconscious, not Dead (raw -2 > kDeathOvershoot -5)");
    }

    // --- 10. Alive -> Dead directly: a batch stacking two damage effects on
    //         the same target in one turn crosses kDeathOvershoot ---------
    {
        World hurt = world;
        Entity* guard = find_entity(hurt, "guard_01");
        assert(guard);
        guard->hp = 0;  // already down, but vital defaults to Alive here -- proves Alive can skip
                        // straight to Dead on a big enough combined blow, not just via Unconscious

        std::vector<Effect> batch = {
            Damage{{"player"}, {"guard_01"}, {"dagger"}, 999},     // clamps to 4
            Damage{{"player"}, {"guard_01"}, {"oil_flask"}, 999},  // clamps to 2
        };
        auto result = gauntlet::run_gauntlet(hurt, batch);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);

        World after = applier::apply(hurt, *effects);
        Entity* guard_after = find_entity(after, "guard_01");
        assert(guard_after);
        check(guard_after->vital == VitalState::Dead,
              "dagger+oil_flask in one batch from 0 hp (raw -6) crosses kDeathOvershoot -> Dead, skipping Unconscious");
    }

    // --- 11. Unconscious -> Dead: the same combined blow also finishes off
    //         a target that was already Unconscious, not just Alive -------
    {
        World hurt = world;
        Entity* guard = find_entity(hurt, "guard_01");
        assert(guard);
        guard->hp = 0;
        guard->vital = VitalState::Unconscious;

        std::vector<Effect> batch = {
            Damage{{"player"}, {"guard_01"}, {"dagger"}, 999},
            Damage{{"player"}, {"guard_01"}, {"oil_flask"}, 999},
        };
        auto result = gauntlet::run_gauntlet(hurt, batch);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);

        World after = applier::apply(hurt, *effects);
        Entity* guard_after = find_entity(after, "guard_01");
        assert(guard_after);
        check(guard_after->vital == VitalState::Dead,
              "Unconscious -> Dead: same kDeathOvershoot threshold finishes off an already-downed target");
    }

    // --- 12. Unconscious -> Alive: first heal that clears 0 wakes the
    //         entity, no separate threshold -------------------------------
    {
        World hurt = world;
        Entity* guard = find_entity(hurt, "guard_01");
        assert(guard);
        guard->hp = 0;
        guard->vital = VitalState::Unconscious;

        std::vector<Effect> batch = {ModifyResource{{"guard_01"}, ModifyResource::Resource::Hp, +5}};
        auto result = gauntlet::run_gauntlet(hurt, batch);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);

        World after = applier::apply(hurt, *effects);
        Entity* guard_after = find_entity(after, "guard_01");
        assert(guard_after);
        check(guard_after->hp == 5, "Unconscious -> Alive: heal applies normally");
        check(guard_after->vital == VitalState::Alive, "Unconscious -> Alive: first heal past 0 wakes the entity");
    }

    // --- 13. Dead is terminal: derive_vital_state never produces a way out,
    //         even against a massive positive raw total -------------------
    {
        check(rules::derive_vital_state(VitalState::Dead, 999) == VitalState::Dead,
              "derive_vital_state: Dead is terminal for v0, no resurrection path exists yet");
    }

    // --- 14. Player reaches the same three states an entity does ---------
    {
        std::vector<Effect> batch = {ModifyResource{{"player"}, ModifyResource::Resource::Hp, -999}};
        auto result = gauntlet::run_gauntlet(world, batch);  // player starts at 20/20 hp, Alive
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);

        World after = applier::apply(world, *effects);
        check(after.player.hp == 0, "player hp floors at 0, same as an entity");
        check(after.player.vital == VitalState::Dead, "player reaches Dead exactly like an entity (same state set)");
        check(rules::session_over(after), "session_over(): true once the player is Dead");
        check(!rules::session_over(world), "session_over(): false on the untouched starting world");
    }

    // --- 15. Overkill magnitude: captured at apply time, not reconstructed
    {
        World hurt = world;
        Entity* guard = find_entity(hurt, "guard_01");
        assert(guard);
        guard->hp = 0;

        std::vector<Effect> batch = {
            Damage{{"player"}, {"guard_01"}, {"dagger"}, 999},
            Damage{{"player"}, {"guard_01"}, {"oil_flask"}, 999},
        };
        auto result = gauntlet::run_gauntlet(hurt, batch);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);

        auto transitions = applier::vital_transitions(hurt, *effects);
        assert(transitions.size() == 1);
        check(transitions[0].entity == EntityId{"guard_01"}, "vital_transitions: names the affected entity");
        check(transitions[0].from == VitalState::Alive && transitions[0].to == VitalState::Dead,
              "vital_transitions: records the Alive -> Dead transition");
        check(transitions[0].overkill == -6,
              "vital_transitions: overkill is the batch's raw total (0 - 4 - 2 = -6), stored directly");
    }

    // --- 16. vital_transitions is empty on an ordinary, non-combat turn ---
    {
        auto result = gauntlet::run_gauntlet(world, examples::bribe_the_guard().effects_on_success);
        auto* effects = std::get_if<std::vector<Effect>>(&result);
        assert(effects);
        auto transitions = applier::vital_transitions(world, *effects);
        check(transitions.empty(), "vital_transitions: empty when no hp-affecting effect fired this turn");
    }

    std::cout << "\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
