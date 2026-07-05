#pragma once
#include "gm_effects.hpp"
#include "gm_state.hpp"
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// GM Adjudicator — v0 validator reference data.
//
// Hardcoded C++ tables and small pure lookup helpers the validator
// gauntlet (gm_gauntlet.hpp) consults during its "preconditions met" and
// "magnitude in bounds" stages (gm-adjudicator.md §4, "The Acceptance
// Gauntlet"). This header holds no gauntlet control flow itself — it is
// data plus lookups, kept separate from gm_state.hpp (the state *model*)
// and gm_effects.hpp (the effect *vocabulary*) because none of this is
// part of either contract; it's the room-specific knowledge the validator
// needs to enforce that contract against the guardroom scenario.
//
// Storage choice (locked): hardcoded C++ rather than JSON, matching the
// "representative, not final" spirit already locked for the effect
// vocabulary itself (gm-adjudicator.md §3 Non-Goals). Promote to a
// data-driven format the moment a second room exists and hand-editing
// this file starts to hurt — not before (the project's own kill-switch
// logic: don't build for scale before Experiment 1 proves the loop is
// fun; context doc §6).

namespace gm::rules {

// ---------------------------------------------------------------------
// Damage cap table
// ---------------------------------------------------------------------
// Every item that can exist in the current room's state must have an
// entry here — no runtime fallback for a missing one. A lookup miss is a
// *table gap* (someone forgot to cover a real item the room can produce),
// which is a different failure from "item doesn't exist" — that's
// gauntlet stage 2, the hallucination check, and happens earlier, on a
// different id. Conflating the two would hide a real authoring bug behind
// a hallucination label.
//
// Every physical item in the room gets a row, whether or not combat is
// its "intended" use — a key to the face still hurts, oil that's already
// lit still burns skin. This mirrors the project's Brogue-style
// composability bet (context doc §3) and the locked "closed vocabulary,
// no deny-unknown" stance already applied to effects (gm-adjudicator.md
// §4), one level down: applied here to weapons instead of verbs.
inline const std::map<std::string, int64_t> kDamageCap = {
    {"dagger",      4},  // the sample weapon named in the spec's own Damage comment
    {"oil_flask",   2},  // glass + fire; worse than a blade as a swung weapon
    {"key_north",   1},  // blunt improvised object — comic-weak on purpose (context doc §7)
    {"__unarmed__", 1},  // UNARMED_ITEM; fists. A stated table row, not a fallback.
};

// Looks up the cap for the item named by Damage::via. Returns nullopt on
// a miss so the caller (the validator) can distinguish "no cap defined
// for this item" (a table gap — reject and flag for the table author)
// from "capped at zero" (a real, intentional value).
inline std::optional<int64_t> damage_cap(const ItemId& via) {
    auto it = kDamageCap.find(via.value);
    if (it == kDamageCap.end()) return std::nullopt;
    return it->second;
}

// ---------------------------------------------------------------------
// entity_exists / object_exists / item_exists — the "ids exist" stage
// ---------------------------------------------------------------------
// The hallucination check (gauntlet stage 2, gm-adjudicator.md §4): does
// this id resolve to something in current state at all? Deliberately NOT
// "does X hold Y" (that's holds(), below — a precondition, stage 3) and
// NOT "is X eligible for this particular field" (that's
// disposition_eligible/resource_eligible — also stage 3). Three separate
// questions that happen to all be "is this a real id" on the surface but
// fail at different gauntlet stages for different reasons, per session
// log 4 §2.1: a table gap and a hallucination must not be reported under
// the same label, and the same discipline applies here — an id that
// exists but isn't eligible for a field is not a hallucination.
//
// These are intentionally strict: VOID_ENTITY and UNARMED_ITEM are real
// sentinels but are NOT real state ids (gm_effects.hpp), so
// entity_exists(VOID_ENTITY) and item_exists(UNARMED_ITEM) both correctly
// return false here. The carve-out documented below is the gauntlet
// control flow's job — checking the sentinel explicitly before calling
// these — not this header's. Baking the carve-out into the helpers
// themselves would make "exists" quietly mean two different things
// depending on caller, which is exactly the kind of silent bypass this
// project has already burned itself on once (Damage.via).
inline bool entity_exists(const World& world, const EntityId& id) {
    if (id == world.player.id) return true;
    for (const auto& entity : world.entities)
        if (entity.id == id) return true;
    return false;
}

inline bool object_exists(const World& world, const ObjectId& id) {
    for (const auto& object : world.objects)
        if (object.id == id) return true;
    return false;
}

// item_exists only proves the id is real *somewhere* in current state —
// held by the player or by any entity right now. It does NOT prove the
// effect's named holder currently has it; that's holds(), a later,
// separate precondition stage. An item id that exists but sits in the
// wrong hands must fail as "you don't have that", not "no such item".
inline bool item_exists(const World& world, const ItemId& id) {
    for (const auto& held : world.player.inventory)
        if (held == id) return true;
    for (const auto& entity : world.entities)
        for (const auto& held : entity.holds)
            if (held == id) return true;
    return false;
}

// ---------------------------------------------------------------------
// holds() — unifies the Player.inventory / Entity.holds asymmetry
// ---------------------------------------------------------------------
// Player and Entity store carried items under different field names on
// different structs (gm_state.hpp). Two gauntlet precondition checks need
// "does this id hold this item" — TransferItem's `from` precondition, and
// Damage's `via` precondition (the source must actually be holding the
// weapon it names) — so the lookup lives here once, rather than being
// duplicated at each call site and drifting out of sync later.
inline bool holds(const World& world, const EntityId& id, const ItemId& item) {
    if (id == world.player.id) {
        for (const auto& held : world.player.inventory)
            if (held == item) return true;
        return false;
    }
    for (const auto& entity : world.entities) {
        if (entity.id == id) {
            for (const auto& held : entity.holds)
                if (held == item) return true;
            return false;
        }
    }
    return false;  // id not found here; existence is gauntlet stage 2's job, not this helper's
}

// ---------------------------------------------------------------------
// Effect -> id-class eligibility
// ---------------------------------------------------------------------
// Not every EntityId-typed field can legally resolve to the player. E.g.
// "player" passes the existence check cleanly (it resolves to
// World::player), but Player has no `disposition` field the way Entity
// does — so modify_disposition(player, +1) would sail through the
// hallucination check and then have nothing to modify at apply time.
//
// modify_disposition is the *only* v0 effect with this restriction.
// Everything else that targets an EntityId (Damage::source/target,
// ModifyResource::actor, TransferItem::from/to) accepts either the player
// or an entity — their fields (hp, holds/inventory) exist on both structs.
inline bool disposition_eligible(const World& world, const EntityId& id) {
    if (id == world.player.id) return false;  // Player has no disposition field to modify
    for (const auto& entity : world.entities)
        if (entity.id == id) return true;
    return false;  // not found here either; existence is a separate, earlier gauntlet stage
}

// ---------------------------------------------------------------------
// resource_eligible() — the modify_resource(gold) counterpart
// ---------------------------------------------------------------------
// Surfaced while building the gauntlet control flow (not previously
// named): the mirror image of disposition_eligible's gap. `gold` exists
// only on Player, not Entity (gm_state.hpp) — so modify_resource(guard_01,
// "gold", -5) would sail through the existence check exactly the way
// modify_disposition(player, +1) used to, and then have no field to
// modify at apply time. `hp` exists on both structs, so it has no such
// restriction. Same shape as disposition_eligible on purpose: one
// eligibility rule per field that isn't universal across id-classes,
// checked at the preconditions stage, never the existence stage.
inline bool resource_eligible(const World& world, const EntityId& id, ModifyResource::Resource resource) {
    if (resource == ModifyResource::Resource::Hp) return true;  // hp exists on both Player and Entity
    return id == world.player.id;  // gold exists only on Player
}

// ---------------------------------------------------------------------
// modify_resource(gold) — cannot go negative
// ---------------------------------------------------------------------
// Locked: spending more gold than the actor has is a rejected
// precondition, not a clamp — symmetric to transfer_item's "from must
// hold item" (you can't give away money, any more than an item, that you
// don't have). This is why workorder_bribe.json was corrected from -20 to
// -15: the player starts with 15 gold (room_guardroom.json), and the
// bribe's own modifier label ("generous offer on a 15-gold purse") had
// already assumed that balance.
inline bool gold_spend_legal(const World& world, int64_t delta) {
    if (delta >= 0) return true;  // gains are never rejected on this ground
    return world.player.gold + delta >= 0;
}

// ---------------------------------------------------------------------
// set_object_state — instance-is-schema property check
// ---------------------------------------------------------------------
// Locked approach: an object's legal properties are exactly the keys it
// was seeded with in the room JSON — no separate per-object-type schema
// table yet. A set_object_state effect is legal only if `property`
// already exists on the target ObjectState, and `value`'s variant
// alternative matches the existing value's alternative (a bool-valued
// property can't be overwritten with a string). This is free — it reuses
// room-authored data instead of adding a new schema structure — at the
// cost of not generalizing once two objects of the same conceptual "type"
// want different legal properties; that trade is deliberately deferred to
// whenever room #2 makes it hurt.
inline bool object_property_legal(const ObjectState& object,
                                   const std::string& property,
                                   const PropertyValue& value) {
    auto it = object.properties.find(property);
    if (it == object.properties.end()) return false;  // property wasn't seeded on this object
    return it->second.index() == value.index();        // same variant alternative (type match)
}

// ---------------------------------------------------------------------
// Preconditions resolved as "allow, no check needed"
// ---------------------------------------------------------------------
// Documented here for completeness rather than as functions, since
// there's nothing for the validator to reject in these cases:
//   - transfer_item(from == to)             -> legal no-op; item stays put.
//   - Damage(source == target)              -> self-damage is legal
//     (poison, traps, stepping on one's own effect).
//   - transfer_item against an Unconscious or Dead entity -> deliberately
//     ungated (gm-adjudicator.md §5): looting/inspecting a downed or dead
//     entity is meant to work, and transfer_item's only precondition was
//     ever holds() — this is a locked decision to add NO new restriction
//     here, not an oversight.

// ---------------------------------------------------------------------
// Vital state — Alive / Unconscious / Dead (gm-adjudicator.md §5, "hp
// ceiling and alive/dead state")
// ---------------------------------------------------------------------
inline VitalState vital_state(const World& world, const EntityId& id) {
    if (id == world.player.id) return world.player.vital;
    for (const auto& entity : world.entities)
        if (entity.id == id) return entity.vital;
    return VitalState::Alive;  // unreachable post-gauntlet; existence is a separate, earlier stage
}

inline int64_t hp_of(const World& world, const EntityId& id) {
    if (id == world.player.id) return world.player.hp;
    for (const auto& entity : world.entities)
        if (entity.id == id) return entity.hp;
    return 0;  // unreachable post-gauntlet
}

// A single item's damage cap tops out at 4 (kDamageCap, dagger) — no lone
// hit can reach this threshold alone; only a batch that stacks multiple
// hp-affecting effects onto one target in the same work order, or a
// second finishing blow after an entity is already at 0, can cross it.
// Deliberately hard to reach by accident, easy to reach on purpose (spec
// §5, worked discussion).
inline constexpr int64_t kDeathOvershoot = -5;

// Pure derivation, no World mutation and no GM-authored input anywhere:
// vital state is never proposed by a work order (there is no
// set_vital_state effect), so unlike everything else in this file this
// isn't validating untrusted input — it's engine-internal math, which is
// why it lives here as a plain function rather than as a gauntlet
// precondition (that split is itself locked, spec §5). Dead is terminal
// for v0 — recoverable only by a future item/magic feature that doesn't
// exist yet; named here so the missing exit edge reads as a decision, not
// an oversight, the same discipline as every other deferred gap in this
// file.
inline VitalState derive_vital_state(VitalState previous, int64_t raw_hp_total) {
    if (previous == VitalState::Dead) return VitalState::Dead;
    if (raw_hp_total <= kDeathOvershoot) return VitalState::Dead;
    if (raw_hp_total <= 0) return VitalState::Unconscious;
    return VitalState::Alive;  // covers Unconscious -> Alive: any heal that clears 0 wakes the entity, no extra threshold
}

struct VitalDelta {
    EntityId entity;
    VitalState previous;
    VitalState next;
    int64_t raw_hp_total;  // hp-before-this-turn plus the batch's cumulative
                            // raw (pre-floor, pre-ceiling) delta — only
                            // meaningful as an overshoot number once `next`
                            // lands on Unconscious or Dead.
};

// Batch-level, not per-effect: two effects targeting the same entity in
// one turn (e.g. a dagger swing AND an oil-flask burn in the same work
// order) must sum before comparing against kDeathOvershoot, or the
// per-effect hp floor/ceiling already applied by the applier
// (gm_applier.hpp) erases the signal a second effect would need to see.
// Computed here, against `effects` alone plus the World *before* this
// turn — reused by both gm_applier.hpp (to derive the stored vital_state)
// and the log (gm_log.hpp's Applied.vital_transitions), so the two can't
// drift out of sync with each other (same shape as
// modifier_magnitude_sum, above).
inline std::vector<VitalDelta> vital_deltas(const World& world, const std::vector<Effect>& effects) {
    std::vector<std::pair<EntityId, int64_t>> raw;
    auto accumulate = [&](const EntityId& id, int64_t delta) {
        for (auto& [eid, total] : raw)
            if (eid == id) { total += delta; return; }
        raw.push_back({id, delta});
    };
    for (const auto& effect : effects) {
        if (const auto* dmg = std::get_if<Damage>(&effect)) {
            accumulate(dmg->target, -dmg->amount);
        } else if (const auto* res = std::get_if<ModifyResource>(&effect)) {
            if (res->resource == ModifyResource::Resource::Hp)
                accumulate(res->actor, res->delta);
        }
    }

    std::vector<VitalDelta> deltas;
    deltas.reserve(raw.size());
    for (const auto& [id, delta] : raw) {
        VitalState previous = vital_state(world, id);
        int64_t raw_total = hp_of(world, id) + delta;
        deltas.push_back({id, previous, derive_vital_state(previous, raw_total), raw_total});
    }
    return deltas;
}

// hp-affecting-effect eligibility — Dead is terminal (derive_vital_state
// above): a Dead target can no longer be damaged or healed by either
// `damage` or `modify_resource(hp)`. Unconscious remains fully legal on
// both sides — a finishing blow and a revival heal are both ordinary hp
// effects right up until Dead.
inline bool hp_effect_legal(const World& world, const EntityId& id) {
    return vital_state(world, id) != VitalState::Dead;
}

// modify_disposition's second eligibility rule, alongside
// disposition_eligible() above. Kept as a separate function rather than
// merged into it: the two reject for different reasons ("wrong id-class"
// vs. "not alive"), the same discipline session log 4 already applied to
// keep a table gap and a hallucination from sharing one failure label.
inline bool disposition_alive(const World& world, const EntityId& id) {
    return vital_state(world, id) == VitalState::Alive;
}

// True once the player is Dead. NOT wired to a turn loop yet:
// wizard_of_oz_demo.cpp runs a single hardcoded turn, not a real
// multi-turn loop — building that loop is already separately named as
// pending work. This is the rule the eventual loop calls to stop;
// correctness is proven here and in test_applier.cpp ahead of that loop
// existing, so the loop's own future change is wiring, not new logic.
inline bool session_over(const World& world) {
    return world.player.vital == VitalState::Dead;
}

// ---------------------------------------------------------------------
// modify_resource(hp) vs. damage — a convention, not an enforced rule
// ---------------------------------------------------------------------
// modify_resource(actor, "hp", delta) is reserved for untargeted causes
// (traps, potions). Any hp change resulting from one entity acting on
// another must be authored as `damage`, so it stays subject to the
// weapon cap above. The validator has no mechanism to *detect* a GM
// routing an attack through modify_resource instead of damage — this is
// an authoring convention for whoever fills out work orders (human now,
// model later), not a gauntlet check. Recorded here so it isn't lost.

// ---------------------------------------------------------------------
// Roll scheme constants (spec §5 "modifier -> roll scheme", locked)
// ---------------------------------------------------------------------
// Approach A (d20 + modifiers vs. a fixed DC), locked in favor of a
// probability-curve alternative for legibility: "sum +5 needs a 10+" is
// readable by a human GM at the table without running code. Same storage
// posture as kDamageCap above -- hardcoded, representative-not-final,
// promote to data-driven only once a second room's tuning needs make
// hand-editing this file hurt.
//
// The two constants are not independent: kModifierBudget is derived FROM
// kDifficultyClass, not picked separately. On a single d20, kDifficultyClass
// = 15 means the success target is (15 - modifier_sum); kModifierBudget = 5
// is the *largest* value that keeps both directions strictly probabilistic
// -- best case needs a 10+ (55%), worst case needs a 20 (5%). One point
// higher and the best case would need <1, i.e. every roll succeeds,
// silently reopening the exact "GM guarantees the outcome" loophole the
// budget exists to close (mirrors the Damage.via lesson: an unbounded
// field is a silent bypass waiting to happen, just relocated from a
// single value to a sum). Kept as two named constants rather than one
// derived at compile time so both are visible and independently
// grep-able at their own call sites (gauntlet's modifier-budget stage;
// gm_roll.hpp's resolve()).
inline constexpr int32_t kDifficultyClass = 15;
inline constexpr int64_t kModifierBudget = 5;

// modifier_magnitude_sum() -- single source of truth for "how much is
// proposed" by a WorkOrder's modifier list. Bounded by MAGNITUDE (sum of
// |value|), not net sum: capping only the net positive total would still
// let a GM stack an unbounded pile of negative modifiers to force a
// guaranteed failure without ever tripping a positive-side cap -- same
// loophole, opposite direction. Magnitude closes both directions with one
// rule and still allows a mixed batch (e.g. +3 / -2) to spend from the
// same pool.
//
// Two call sites need this number, not just a bool: modifier_budget_legal()
// below (a yes/no check), and gm_gauntlet.hpp's validate_modifiers() (which
// also needs the actual sum to report in ModifierBudgetFailure's reason —
// "rejected, was 8 against a budget of 5", not just "rejected"). Previously
// each computed it with its own copy of the same loop; factored here once
// so the two can't drift out of sync with each other.
inline int64_t modifier_magnitude_sum(const std::vector<Modifier>& modifiers) {
    int64_t magnitude_sum = 0;
    for (const auto& m : modifiers) magnitude_sum += std::abs(static_cast<int64_t>(m.value));
    return magnitude_sum;
}

// modifier_budget_legal() -- the roll scheme's own precondition, checked
// against WorkOrder.modifiers as a whole rather than any single Effect.
// Spend is "<=", not exact: a GM with only one weak, honestly-justified
// modifier shouldn't be forced to invent a second one just to use up the
// pool.
inline bool modifier_budget_legal(const std::vector<Modifier>& modifiers) {
    return modifier_magnitude_sum(modifiers) <= kModifierBudget;
}

// ---------------------------------------------------------------------
// Existence-check carve-out (a validator implementation note, not data)
// ---------------------------------------------------------------------
// VOID_ENTITY and UNARMED_ITEM (gm_effects.hpp) are sentinels, not real
// ids — they will never appear in World::entities or any room's item
// list. The gauntlet's "referenced ids exist" stage must special-case
// both explicitly, or the canonical burn_the_door() example
// (transfer_item(..., to=VOID_ENTITY)) and any Damage using UNARMED_ITEM
// will fail the hallucination check by construction.

}  // namespace gm::rules
