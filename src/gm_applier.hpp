#pragma once
#include "gm_effects.hpp"
#include "gm_rules.hpp"
#include "gm_state.hpp"
#include <algorithm>
#include <cstdint>
#include <variant>
#include <vector>

// GM Adjudicator — v0 applier.
// Realizes the [APPLY] seam marked in wizard_of_oz_demo.cpp since session
// log 3 and named the sole remaining critical-path blocker in session log 5.
//
// apply(world, effects) takes ONE already gauntlet-approved vector<Effect>
// (the branch the roll — gm_roll.hpp — selected, or the unconditional
// success branch on a no-roll turn — same "one branch, caller's job to
// pick it" posture gm_gauntlet.hpp already takes) and returns a NEW World
// with the deltas applied.
//
// Precondition, not enforced here: `effects` must already have survived
// gm_gauntlet.hpp's run_gauntlet — ids exist, preconditions met, weapon-cap
// and disposition-range magnitudes already clamped. Apply performs NO
// validation and cannot fail. Calling it on an un-gauntleted batch is a
// caller bug, not something this header defends against — the same
// division of labor gm_json.hpp already draws ("call it after schema
// validation, not before").
//
// Pure function, not in-place mutation (locked session log 6) — matches
// run_gauntlet's own shape (`const World&` in, a new value out) rather
// than introducing an in-place-mutation style found nowhere else in this
// codebase. Costs one State copy per turn; irrelevant at this room's size.
//
// Two clamps are done here, not by the gauntlet: the hp floor and ceiling
// (locked session log 6 and gm-adjudicator.md §5 "hp ceiling and
// alive/dead state"). The gauntlet's "magnitude in bounds" stage only
// clamps a Damage's `amount` against the weapon-cap table (gm_rules.hpp)
// — a property of the weapon, known before hp is ever touched. The floor
// and ceiling (resulting hp in [0, max_hp]) are a property of hp itself,
// not of which effect changed it, so both are applied once, here,
// uniformly across BOTH `damage` and `modify_resource(hp)` — a trap's
// untargeted hp delta can drive an entity out of range exactly the same
// way a sword or a potion can; there's no principled reason to clamp one
// and not the other.
//
// Vital state (Alive/Unconscious/Dead) is a separate concern from this
// per-effect clamp, deliberately NOT derived here inside adjust_hp: it
// depends on the whole turn's cumulative raw delta against hp *before*
// this turn, which per-effect flooring/ceiling already erases by the time
// a second effect in the same batch would need to see it. See apply()
// below and gm_rules.hpp::vital_deltas for where and why that's computed
// instead.

namespace gm::applier::detail {

inline void adjust_hp(World& world, const EntityId& id, int64_t delta) {
    if (id == world.player.id) {
        world.player.hp = std::clamp<int64_t>(world.player.hp + delta, 0, world.player.max_hp);
        return;
    }
    for (auto& entity : world.entities) {
        if (entity.id == id) {
            entity.hp = std::clamp<int64_t>(entity.hp + delta, 0, entity.max_hp);
            return;
        }
    }
    // Unreachable for a gauntlet-approved batch: id existence was already
    // gauntlet stage 2. No-op rather than throw — this header trusts its
    // precondition rather than re-deriving the gauntlet's own checks.
}

inline void set_vital(World& world, const EntityId& id, VitalState v) {
    if (id == world.player.id) { world.player.vital = v; return; }
    for (auto& entity : world.entities)
        if (entity.id == id) { entity.vital = v; return; }
    // Unreachable for a gauntlet-approved batch, same posture as adjust_hp above.
}

inline std::vector<ItemId>* holder_items(World& world, const EntityId& id) {
    if (id == world.player.id) return &world.player.inventory;
    for (auto& entity : world.entities)
        if (entity.id == id) return &entity.holds;
    return nullptr;  // unreachable post-gauntlet; see adjust_hp's note above
}

inline Entity* find_entity(World& world, const EntityId& id) {
    for (auto& entity : world.entities)
        if (entity.id == id) return &entity;
    return nullptr;
}

inline ObjectState* find_object(World& world, const ObjectId& id) {
    for (auto& object : world.objects)
        if (object.id == id) return &object;
    return nullptr;
}

// -- one apply_one overload per effect type, dispatched via std::visit ----
// Mirrors gm_gauntlet.hpp's "one free function per (effect type, stage)"
// shape, so this header reads as a checklist against the effect
// vocabulary (gm_effects.hpp) the same way the gauntlet reads as a
// checklist against its stage list.

inline void apply_one(World& world, const Damage& e) {
    adjust_hp(world, e.target, -e.amount);  // e.amount already weapon-cap-clamped by the gauntlet
}

inline void apply_one(World& world, const ModifyDisposition& e) {
    // e.delta is already the clamped EFFECTIVE delta by the time it
    // reaches here (gm_gauntlet.hpp's clamp_disposition_delta computes
    // target - current, not the raw proposed delta) — plain addition is
    // correct, no second clamp needed.
    if (Entity* entity = find_entity(world, e.entity))
        entity->disposition += e.delta;
}

inline void apply_one(World& world, const TransferItem& e) {
    if (e.from == e.to) return;  // legal no-op (gm_rules.hpp: from==to precondition note)

    if (auto* from_items = holder_items(world, e.from)) {
        auto it = std::find(from_items->begin(), from_items->end(), e.item);
        if (it != from_items->end()) from_items->erase(it);
    }
    if (e.to == VOID_ENTITY) return;  // consumed/destroyed — no destination to add to
    if (auto* to_items = holder_items(world, e.to))
        to_items->push_back(e.item);
}

inline void apply_one(World& world, const ModifyResource& e) {
    if (e.resource == ModifyResource::Resource::Gold) {
        // Gauntlet's gold_spend_legal already rejected any delta that
        // would go negative — no floor needed here, unlike hp.
        world.player.gold += e.delta;
        return;
    }
    adjust_hp(world, e.actor, e.delta);  // hp: same floor as Damage, see header comment above
}

inline void apply_one(World& world, const SetObjectState& e) {
    if (ObjectState* object = find_object(world, e.object))
        object->properties[e.property] = e.value;
}

inline void apply_one(World& world, const SetFlag& e) {
    world.flags[e.key] = e.value;  // previously nowhere to land; see World::flags (gm_state.hpp)
}

}  // namespace gm::applier::detail

namespace gm::applier {

inline World apply(const World& world, const std::vector<Effect>& effects) {
    World out = world;
    for (const auto& effect : effects)
        std::visit([&](const auto& e) { detail::apply_one(out, e); }, effect);

    // Vital state derives from the whole batch's raw hp delta against hp
    // *before this turn* (gm_rules.hpp::vital_deltas), not from `out`'s
    // already-floored/ceilinged hp — see that function's own comment and
    // the header comment above adjust_hp.
    for (const auto& delta : rules::vital_deltas(world, effects))
        detail::set_vital(out, delta.entity, delta.next);

    return out;
}

// Only the entities whose vital_state actually changed this turn — most
// turns (no combat) produce an empty vector. `overkill` is the batch's
// raw hp total when it landed the entity on Dead (a negative number, how
// far past kDeathOvershoot — gm_rules.hpp — the blow went); left at 0 for
// a transition into Unconscious, since "how far below 0" isn't overkill
// in that sense until Dead is actually reached.
//
// Not reconstructed later from world_before/world_after: gm_rules.hpp's
// vital_deltas already computes this number to decide the new state in
// the first place (gm-adjudicator.md §5, "hp ceiling and alive/dead
// state" — storing it now, while apply() is already standing at the
// point it's computed, beats a future replay pass re-deriving it).
// Separate from apply() itself (rather than folded into a richer return
// type) so apply()'s existing "World in, World out" signature — and every
// call site already built on it — stays untouched; the caller building a
// log entry (gm_log.hpp's Applied) is the only consumer that needs this.
struct VitalTransition {
    EntityId entity;
    VitalState from;
    VitalState to;
    int64_t overkill;
};

inline std::vector<VitalTransition> vital_transitions(const World& world_before,
                                                        const std::vector<Effect>& effects) {
    std::vector<VitalTransition> transitions;
    for (const auto& delta : rules::vital_deltas(world_before, effects)) {
        if (delta.previous == delta.next) continue;
        int64_t overkill = (delta.next == VitalState::Dead) ? delta.raw_hp_total : 0;
        transitions.push_back({delta.entity, delta.previous, delta.next, overkill});
    }
    return transitions;
}

}  // namespace gm::applier
