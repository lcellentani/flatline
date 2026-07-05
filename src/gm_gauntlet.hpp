#pragma once
#include "gm_applier.hpp"
#include "gm_effects.hpp"
#include "gm_state.hpp"
#include "gm_rules.hpp"
#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// GM Adjudicator — v0 validator gauntlet control flow.
//
// The sole remaining blocker on the critical path per session log 4 §4:
// gm_rules.hpp holds the reference *data* the gauntlet consults; this
// header is the *sequencing* — the function that walks an already-
// roll-selected effect branch through the ordered stages named in
// gm-adjudicator.md §4 ("The Acceptance Gauntlet") and reports which
// effect failed, and why.
//
// Locked scope for this pass (gm-adjudicator.md, "Gauntlet validates only
// the roll-selected branch; fails fast per effect"):
//   - Takes ONE branch (a vector<Effect>), not a WorkOrder. Branch
//     selection (the roll — gm_roll.hpp; "no roll" -> success is
//     automatic) is the caller's job, not this header's — orthogonal to
//     validation by design, independent of the roll scheme itself having
//     since been locked (session log 7).
//   - Stops at the FIRST effect that fails ANY stage. No partial batch
//     ever reaches the applier (gm_applier.hpp).
//   - Known, named gap (spec §5, "Unvalidated branch hides Experiment 2
//     failures"): whichever branch the caller does NOT pass in is never
//     run through this at all. Accepted for now; revisit once Experiment
//     2 tooling exists.
//
// Session log 11 ("Same-batch effect conflicts", spec §5): the
// preconditions stage now folds state forward across the branch instead of
// checking every effect against the same static pre-turn `world`. Two
// effects that independently look legal against a snapshot neither of them
// has actually mutated yet (two `transfer_item`s moving one item to two
// destinations; two `modify_resource(gold)` spends that individually fit
// the balance but not both together) now see each other. This is a
// revision of the previously-locked "preconditions checked against a
// single pre-turn snapshot" line in gm-adjudicator.md §4 — named as a
// revision, not a silent extension.
//   - `ids_exist` deliberately stays checked against the untouched
//     `world`, not the folded state — `item_exists` (gm_rules.hpp) proves
//     an id is real *somewhere in current holder state*, so an item a
//     prior effect already consumed to VOID_ENTITY would otherwise
//     misreport as a hallucination instead of a same-batch conflict. No
//     effect in the v0 vocabulary creates or deletes an id, so checking
//     existence against the pre-turn snapshot is never stale.
//   - Still whole-batch rejection, still no partial application — folding
//     is internal to validation only; `apply()` (gm_applier.hpp) is
//     untouched and still applies the gauntlet-approved batch in one
//     atomic pass against the real pre-turn `World`. "Apply atomically" is
//     preserved; what changed is how legality is *checked*, not how the
//     batch is *applied*.
//   - A same-batch conflict is reported as its own `Stage::BatchConflict`,
//     not folded into `Stage::Preconditions` — a precondition that was
//     never legal even at turn start and one that a sibling effect in this
//     same batch broke are different failure shapes with different
//     resubmission fixes (re-author the effect vs. reorder/drop a
//     colliding one), same discipline session log 4 §2.1 already applied
//     to keep a table gap and a hallucination from sharing one label.
//     `Failure::conflicting_with_index` names the earlier effect
//     responsible, so whoever resubmits doesn't have to guess.
//   - Rejected alternatives (session log 10 → 11): a dedicated
//     batch-level pre-check stage scanning for same-resource claims before
//     anything runs (doesn't generalize past the cases it's built for —
//     would have missed the `modify_disposition` double-clamp below);
//     authoring-convention-only (prior art already rejected this for
//     `modify_resource(hp)` — invisible in Experiment 1, meaningless in
//     Experiment 2); a `NeedsResolution` outcome with interactive
//     mid-turn resolution (reopens the one-call decision session log 1
//     §2.7 already settled — a resolve step is a second inference
//     dependency for Experiment 2, not just Experiment 1's problem to
//     solve for free).
//
// "schema-valid" (gauntlet stage 1) is NOT re-checked here — it's already
// discharged by gm_json.hpp at parse time. A well-formed Effect variant
// cannot fail that stage by the time it reaches this header.
//
// Not implemented here, by design: apply (gm_applier.hpp), log
// (gm_log.hpp), roll (gm_roll.hpp). Each is its own header; all three are
// composed together with this one in wizard_of_oz_demo.cpp.

namespace gm::gauntlet {

enum class Stage : uint8_t { IdsExist, Preconditions, MagnitudeBounds, BatchConflict };

inline std::string stage_name(Stage s) {
    switch (s) {
        case Stage::IdsExist:        return "ids_exist";
        case Stage::Preconditions:   return "preconditions";
        case Stage::MagnitudeBounds: return "magnitude_bounds";
        case Stage::BatchConflict:   return "batch_conflict";
    }
    return "unknown";
}

struct Failure {
    std::size_t effect_index;  // position within the branch passed to run_gauntlet
    Stage stage;
    std::string reason;        // human-legible; keeps hallucination vs. table-gap vs.
                                // precondition distinct per session log 4 §2.1 — never
                                // collapse these into one label
    // Set only when stage == BatchConflict: the index of the earlier effect
    // in this same branch whose application first broke a precondition that
    // was legal against the untouched pre-turn world (session log 11).
    // std::nullopt for every other stage — a single-case optional field,
    // accepted per "promoted to a variant only when a second stage needs
    // the same shape" rather than restructuring Failure itself now.
    std::optional<std::size_t> conflicting_with_index = std::nullopt;
};

// Result: either every effect in the branch survived (post-clamp, ready
// for the applier — gm_applier.hpp) or the FIRST failure encountered — never
// both. A variant makes "ignore `applied` on failure" unrepresentable
// rather than a convention to remember at every call site, the same fix
// shape as Damage.via going from optional-with-a-silent-bypass to
// required-with-a-sentinel (this project's own top named lesson).
using Result = std::variant<std::vector<Effect>, Failure>;

// ---------------------------------------------------------------------
// Per-effect, per-stage checks. One free function per (effect type,
// stage) so run_gauntlet's dispatch reads as a checklist against
// gm-adjudicator.md §4 rather than one large branching function.
// ---------------------------------------------------------------------
namespace detail {

// -- damage ---------------------------------------------------------
inline std::optional<Failure> ids_exist(const World& world, std::size_t idx, const Damage& e) {
    if (!rules::entity_exists(world, e.source))
        return Failure{idx, Stage::IdsExist, "damage.source '" + e.source.value + "' does not exist"};
    if (!rules::entity_exists(world, e.target))
        return Failure{idx, Stage::IdsExist, "damage.target '" + e.target.value + "' does not exist"};
    // Sentinel carve-out (gm_rules.hpp): UNARMED_ITEM is never a real item id.
    if (e.via != UNARMED_ITEM && !rules::item_exists(world, e.via))
        return Failure{idx, Stage::IdsExist, "damage.via '" + e.via.value + "' does not exist"};
    return std::nullopt;
}

inline std::optional<Failure> preconditions(const World& world, std::size_t idx, const Damage& e) {
    if (e.via != UNARMED_ITEM && !rules::holds(world, e.source, e.via))
        return Failure{idx, Stage::Preconditions,
                        "damage.source '" + e.source.value + "' does not hold via '" + e.via.value + "'"};
    // Dead is terminal (gm_rules.hpp::derive_vital_state) — Unconscious remains
    // a legal target (finishing blows are ordinary damage until Dead).
    if (!rules::hp_effect_legal(world, e.target))
        return Failure{idx, Stage::Preconditions,
                        "damage.target '" + e.target.value + "' is already dead"};
    return std::nullopt;
}

// Mutates: clamps amount to via's cap. A lookup miss on the cap table is
// a table gap (gm_rules.hpp), distinct from a hallucination — via already
// passed the existence check above.
inline std::variant<int64_t, Failure> magnitude_bounds(std::size_t idx, const Damage& e) {
    auto cap = rules::damage_cap(e.via);
    if (!cap)
        return Failure{idx, Stage::MagnitudeBounds,
                        "damage.via '" + e.via.value + "' has no damage-cap table entry (table gap)"};
    return std::min(e.amount, *cap);
}

// -- modify_disposition -----------------------------------------------
inline std::optional<Failure> ids_exist(const World& world, std::size_t idx, const ModifyDisposition& e) {
    if (!rules::entity_exists(world, e.entity))
        return Failure{idx, Stage::IdsExist, "modify_disposition.entity '" + e.entity.value + "' does not exist"};
    return std::nullopt;
}

inline std::optional<Failure> preconditions(const World& world, std::size_t idx, const ModifyDisposition& e) {
    if (!rules::disposition_eligible(world, e.entity))
        return Failure{idx, Stage::Preconditions,
                        "modify_disposition.entity '" + e.entity.value + "' is not eligible (player has no disposition field)"};
    // Separate check from the id-class one above (different reason to fail,
    // same discipline session log 4 used to keep a table gap and a
    // hallucination from sharing one label): an Unconscious or Dead entity
    // has no attitude to be modified right now.
    if (!rules::disposition_alive(world, e.entity))
        return Failure{idx, Stage::Preconditions,
                        "modify_disposition.entity '" + e.entity.value + "' is not alive"};
    return std::nullopt;
}

// Clamps the RESULTING disposition to [Hostile, Allied] and returns the
// effective delta (current -> clamped), not the raw one. "Clamped to the
// disposition enum range" (gm_effects.hpp) describes the destination
// value, not the input — mirrors damage's amount-clamp shape.
inline int32_t clamp_disposition_delta(const World& world, const ModifyDisposition& e) {
    int32_t current = 0;
    for (const auto& entity : world.entities)
        if (entity.id == e.entity) { current = entity.disposition; break; }
    int32_t target = current + e.delta;
    if (target < disposition::Hostile) target = disposition::Hostile;
    if (target > disposition::Allied)  target = disposition::Allied;
    return target - current;
}

// -- transfer_item ------------------------------------------------------
inline std::optional<Failure> ids_exist(const World& world, std::size_t idx, const TransferItem& e) {
    if (!rules::item_exists(world, e.item))
        return Failure{idx, Stage::IdsExist, "transfer_item.item '" + e.item.value + "' does not exist"};
    if (!rules::entity_exists(world, e.from))
        return Failure{idx, Stage::IdsExist, "transfer_item.from '" + e.from.value + "' does not exist"};
    // Sentinel carve-out: VOID_ENTITY is never a real entity id.
    if (e.to != VOID_ENTITY && !rules::entity_exists(world, e.to))
        return Failure{idx, Stage::IdsExist, "transfer_item.to '" + e.to.value + "' does not exist"};
    return std::nullopt;
}

inline std::optional<Failure> preconditions(const World& world, std::size_t idx, const TransferItem& e) {
    if (!rules::holds(world, e.from, e.item))
        return Failure{idx, Stage::Preconditions,
                        "transfer_item.from '" + e.from.value + "' does not hold item '" + e.item.value + "'"};
    return std::nullopt;
}

// -- modify_resource ----------------------------------------------------
inline std::optional<Failure> ids_exist(const World& world, std::size_t idx, const ModifyResource& e) {
    if (!rules::entity_exists(world, e.actor))
        return Failure{idx, Stage::IdsExist, "modify_resource.actor '" + e.actor.value + "' does not exist"};
    return std::nullopt;
}

inline std::optional<Failure> preconditions(const World& world, std::size_t idx, const ModifyResource& e) {
    if (!rules::resource_eligible(world, e.actor, e.resource)) {
        const char* res = (e.resource == ModifyResource::Resource::Gold) ? "gold" : "hp";
        return Failure{idx, Stage::Preconditions,
                        std::string("modify_resource.actor '") + e.actor.value + "' is not eligible for resource '" + res + "'"};
    }
    if (e.resource == ModifyResource::Resource::Gold && !rules::gold_spend_legal(world, e.delta))
        return Failure{idx, Stage::Preconditions, "modify_resource(gold) would take the actor negative"};
    // hp: floor/ceiling are the applier's job (gm_applier.hpp), not a
    // precondition — but Dead is terminal either direction, damage or
    // heal, same as Damage's target check above.
    if (e.resource == ModifyResource::Resource::Hp && !rules::hp_effect_legal(world, e.actor))
        return Failure{idx, Stage::Preconditions,
                        "modify_resource(hp).actor '" + e.actor.value + "' is already dead"};
    return std::nullopt;
}

// -- set_object_state -----------------------------------------------------
inline std::optional<Failure> ids_exist(const World& world, std::size_t idx, const SetObjectState& e) {
    if (!rules::object_exists(world, e.object))
        return Failure{idx, Stage::IdsExist, "set_object_state.object '" + e.object.value + "' does not exist"};
    return std::nullopt;
}

inline std::optional<Failure> preconditions(const World& world, std::size_t idx, const SetObjectState& e) {
    const ObjectState* obj = nullptr;
    for (const auto& o : world.objects)
        if (o.id == e.object) { obj = &o; break; }
    // obj is guaranteed non-null here: ids_exist already passed for this effect.
    if (!rules::object_property_legal(*obj, e.property, e.value))
        return Failure{idx, Stage::Preconditions,
                        "set_object_state: property '" + e.property + "' not legal on '" + e.object.value + "'"};
    return std::nullopt;
}

// -- set_flag -------------------------------------------------------------
// Dormant memory hook (gm_effects.hpp) — no ids to check, no
// preconditions defined yet. Always passes.
inline std::optional<Failure> ids_exist(const World&, std::size_t, const SetFlag&) { return std::nullopt; }
inline std::optional<Failure> preconditions(const World&, std::size_t, const SetFlag&) { return std::nullopt; }

}  // namespace detail

// ---------------------------------------------------------------------
// validate_modifiers -- the roll scheme's own gauntlet stage
// ---------------------------------------------------------------------
// Deliberately NOT part of run_gauntlet above: run_gauntlet validates one
// already-SELECTED vector<Effect> branch, but WorkOrder.modifiers is what
// SELECTS the branch in the first place (via the roll, gm_roll.hpp) --
// it has to be checked earlier and separately, against the WorkOrder as
// a whole rather than against either effect branch. Same "one stage, one
// free function" shape as the per-effect stages above, just at a
// different point in the per-turn sequence:
//   validate_modifiers(wo.modifiers) -> [ROLL] -> run_gauntlet(selected branch) -> [APPLY]
//
// A distinct Failure-shaped struct rather than reusing gauntlet::Failure:
// Failure::effect_index means "position within the branch that failed" --
// meaningless here, since there is no branch yet at this point in the
// sequence. A dedicated type keeps "no effect_index applies" structurally
// true instead of a convention to remember at the one call site.
struct ModifierBudgetFailure {
    int64_t magnitude_sum;
    int64_t budget;
    std::string reason;
};

inline std::optional<ModifierBudgetFailure> validate_modifiers(const std::vector<Modifier>& modifiers) {
    // Shared with rules::modifier_budget_legal() — one computation, not two
    // independently-maintained copies of the same loop (see gm_rules.hpp's
    // modifier_magnitude_sum() comment).
    int64_t magnitude_sum = rules::modifier_magnitude_sum(modifiers);
    if (magnitude_sum > rules::kModifierBudget) {
        return ModifierBudgetFailure{
            magnitude_sum, rules::kModifierBudget,
            "modifier magnitude sum " + std::to_string(magnitude_sum) +
                " exceeds budget " + std::to_string(rules::kModifierBudget)};
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------
// run_gauntlet — the ordered sequence, fail-fast (A1+B1, locked)
// ---------------------------------------------------------------------
// `branch` is whichever vector<Effect> the roll (gm_roll.hpp) already
// selected — effects_on_success, effects_on_failure, or the unconditional
// success branch on a no-roll turn. This function does not know or care
// which; that's the point of validating only the selected branch.
inline Result run_gauntlet(const World& world, const std::vector<Effect>& branch) {
    std::vector<Effect> applied;
    applied.reserve(branch.size());

    // Session log 11: `history[k]` is the World as of just before effect k
    // is checked — effects 0..k-1 already folded in. `history[0]` is the
    // untouched pre-turn snapshot. Kept in full (not just a single running
    // scratch value) so that when a same-batch conflict fires, the search
    // below can point at exactly which earlier effect broke it, instead of
    // only reporting that *some* earlier effect did. Purely internal to
    // validation — discarded either way; the real apply() call downstream
    // still runs once, atomically, against the original `world`.
    std::vector<World> history;
    history.reserve(branch.size() + 1);
    history.push_back(world);

    for (std::size_t i = 0; i < branch.size(); ++i) {
        const Effect& effect = branch[i];
        const World& folded = history.back();

        // Stage: referenced ids exist (the hallucination check) — always
        // against the untouched `world`, never `folded`. See header
        // comment: no v0 effect creates or deletes an id, so this can't go
        // stale, and checking it against `folded` would misreport an item
        // a prior effect already consumed (VOID_ENTITY) as a hallucination
        // instead of the same-batch conflict it actually is.
        if (auto failure = std::visit(
                [&](const auto& e) { return detail::ids_exist(world, i, e); }, effect))
            return *failure;

        // Stage: preconditions met — checked against `folded`, this
        // batch's own prior effects already folded in, not the static
        // pre-turn snapshot (session log 11). This is what makes a
        // same-batch conflict visible instead of both colliding effects
        // independently passing against a snapshot neither has mutated yet.
        if (auto failure = std::visit(
                [&](const auto& e) { return detail::preconditions(folded, i, e); }, effect)) {
            // Distinguish "never legal, even at turn start" (an ordinary
            // Preconditions failure — re-check the identical precondition
            // against the untouched world) from "legal at turn start,
            // broken by an earlier effect in this same batch"
            // (BatchConflict).
            bool legal_at_turn_start = !std::visit(
                [&](const auto& e) { return detail::preconditions(world, i, e); }, effect)
                .has_value();
            if (!legal_at_turn_start)
                return *failure;

            // Same-batch conflict: find the earliest prior effect whose
            // application first breaks this precondition, by replaying the
            // identical check against each intermediate state in `history`.
            std::optional<std::size_t> culprit;
            for (std::size_t k = 0; k < i; ++k) {
                bool still_legal = !std::visit(
                    [&](const auto& e) { return detail::preconditions(history[k + 1], i, e); },
                    effect)
                    .has_value();
                if (!still_legal) { culprit = k; break; }
            }
            return Failure{i, Stage::BatchConflict, failure->reason, culprit};
        }

        // Stage: magnitude in bounds. Only damage and modify_disposition
        // mutate anything here; every other effect passes through
        // unchanged — no numeric field the gauntlet clamps. Disposition's
        // clamp now reads `folded`, not `world` — two modify_disposition
        // effects on the same entity in one batch compound correctly
        // instead of both clamping from the same pre-turn starting value.
        Effect out = effect;
        if (auto* dmg = std::get_if<Damage>(&out)) {
            auto clamped = detail::magnitude_bounds(i, *dmg);
            if (auto* fail = std::get_if<Failure>(&clamped)) return *fail;
            dmg->amount = std::get<int64_t>(clamped);
        } else if (auto* disp = std::get_if<ModifyDisposition>(&out)) {
            disp->delta = detail::clamp_disposition_delta(folded, *disp);
        }

        applied.push_back(out);

        World next = folded;
        std::visit([&](const auto& e) { applier::detail::apply_one(next, e); }, out);
        history.push_back(std::move(next));
    }

    return applied;
}

}  // namespace gm::gauntlet
