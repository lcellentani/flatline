#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

// GM Adjudicator — v0 effect vocabulary.
// Realizes gm-adjudicator.md §4 "Effect vocabulary (representative v0)"
// and closes §5 open question #3 ("one model call or two") as one-call:
// the GM authors both outcome branches per turn; the engine rolls once
// (if applicable) and applies the selected branch. See WorkOrder below
// for the full rationale.
//
// This is the language-neutral contract's C++ reader: the same schema a
// human GM fills out by hand now, and a grammar-constrained Gemma 4 E4B
// fills out later (Experiment 2). Nothing here executes anything — these
// are proposed deltas. The validator (not included here) owns the gauntlet:
// schema-valid -> ids exist -> preconditions met -> magnitude in bounds ->
// apply atomically -> log.

namespace gm {

// ---------------------------------------------------------------------
// Strongly-typed ids
// ---------------------------------------------------------------------
// Wrapped strings, not raw std::string and not integer handles yet.
// Rationale: the GM (human or model) only ever sees ids as printed in the
// state snapshot it was handed, and only ever echoes them back as strings.
// Resolving those strings against live state is explicitly the gauntlet's
// "referenced ids exist" step (the hallucination check) — not the GM's
// job and not this header's job. Keeping them as distinct tagged types
// (rather than three loose std::strings) means the compiler catches
// "passed an ObjectId where an EntityId was expected" at the call site,
// which matters once the validator has a dozen effect handlers.
template <typename Tag>
struct Id {
    std::string value;
    bool operator==(const Id&) const = default;
};
struct EntityTag {};
struct ObjectTag {};
struct ItemTag {};

using EntityId = Id<EntityTag>;  // e.g. "guard_01", "player"
using ObjectId = Id<ObjectTag>;  // e.g. "door_north"
using ItemId = Id<ItemTag>;      // e.g. "key_north", "oil_flask"

// Reserved sink entity representing "consumed" / "destroyed". Formalizes
// the spec's informal `<consumed>` target from the burn-the-door worked
// example. transfer_item(x, player, VOID_ENTITY) *is* item destruction —
// deliberately reused rather than adding a destroy_item primitive, per the
// locked "closed vocabulary; novel actions map onto existing primitives"
// decision.
inline const EntityId VOID_ENTITY{"__void__"};

// Reserved sentinel meaning "no weapon" / fists. Damage.via is required
// (never omitted) precisely so an attack can never silently fall through
// to an unscoped "unarmed" default — UNARMED_ITEM makes that case an
// explicit, stated value with its own row in the damage-cap table
// (gm_rules.hpp), the same move as VOID_ENTITY above for "no destination".
inline const ItemId UNARMED_ITEM{"__unarmed__"};

// Reserved sentinel meaning "dealt by an environmental hazard, not
// anything an entity holds" — the same carve-out shape as UNARMED_ITEM
// above, needed the first time Damage.via has to name something nobody
// is holding (being shoved into flame). Named per-hazard (FIRE_ITEM)
// rather than one generic environmental sentinel, so each hazard keeps
// its own row in the damage-cap table instead of sharing a single
// magnitude; generalize to more named sentinels only once a second
// hazard actually needs one, not speculatively now.
inline const ItemId FIRE_ITEM{"__fire__"};

// ---------------------------------------------------------------------
// Property values
// ---------------------------------------------------------------------
// set_object_state touches heterogeneous schema fields (locked: bool,
// material: string, durability: int, ...). One closed value type keeps
// the primitive singular instead of splintering into
// set_bool_state / set_string_state / set_int_state.
using PropertyValue = std::variant<bool, int64_t, std::string>;

// ---------------------------------------------------------------------
// The seven v0 effects
// ---------------------------------------------------------------------
// Representative, not final (spec §3 Non-Goals: "a final, exhaustive
// effect vocabulary" is explicitly out of scope for v0).

struct Damage {
    EntityId source;  // who/what deals it
    EntityId target;
    ItemId via;        // required — the specific item (or UNARMED_ITEM) used to deal the damage.
                       // Resolves the former "which item caps the max when source holds several"
                       // open question: the GM names the instrument explicitly rather than the
                       // validator guessing "best weapon held." Looked up against the closed
                       // damage-cap table in gm_rules.hpp. There is no unscoped "unarmed" default —
                       // UNARMED_ITEM is itself a table row, not a fallback.
    int64_t amount;    // clamped to via's cap (gm_rules.hpp) by the validator
};

struct ModifyDisposition {
    EntityId entity;
    int32_t delta;  // clamped to the disposition enum range by the validator
};

struct TransferItem {
    ItemId item;
    EntityId from;
    EntityId to;  // VOID_ENTITY == consumed
};

struct ModifyResource {
    enum class Resource : uint8_t { Gold, Hp };
    EntityId actor;
    Resource resource;
    int64_t delta;
};

struct SetObjectState {
    ObjectId object;
    std::string property;  // must exist in the object's schema
    PropertyValue value;
};

struct SetFlag {
    // Memory hook — dormant in v0. No validator logic reads flags back yet;
    // this primitive exists solely so memory bolts on later without
    // reworking the contract (context doc §4, "adjudication and memory are
    // a stack, not a fork").
    std::string key;
    PropertyValue value;
};

using Effect = std::variant<
    Damage,
    ModifyDisposition,
    TransferItem,
    ModifyResource,
    SetObjectState,
    SetFlag
>;

// ---------------------------------------------------------------------
// The work order — what the GM emits, once per player turn
// ---------------------------------------------------------------------
enum class Ruling : uint8_t { Allow, Partial, Deny };

struct Modifier {
    std::string label;  // e.g. "bribable", "generous offer on a 15-gold purse"
    int32_t value;       // feeds the engine's seeded roll
};

// One-call decision (spec §5 open question #3, now closed for v0):
// the GM authors BOTH outcome branches in a single call; the engine rolls
// once (if modifiers is non-empty) and applies whichever branch the roll
// selected. Rejected alternative: two calls (modifiers first, effects
// after seeing the roll) — halves per-call schema size but doubles
// inference calls and re-prompt/hallucination exposure per turn, which
// works directly against the platform bet that turn-based play hides
// latency by minimizing model round-trips (context doc §5–6).
//
// `ruling` is NOT the roll's result. It's the GM's scope decision, fixed
// at interpretation time, independent of any dice: of what the player
// asked for, how much is even on the table. E.g. "bribe the guard to open
// the door AND abandon his post" → ruling = partial, because "abandon his
// post" is refused outright — no roll will ever grant it. The roll only
// decides pass/fail on what ruling already left in play.
//
// No-roll turns (modifiers empty, e.g. burn_the_door): the engine performs
// no roll, and effects_on_success/narration_on_success apply
// unconditionally. effects_on_failure/narration_on_failure stay empty —
// there is no failure branch to define.
struct WorkOrder {
    std::string interpretation;   // one line: what the player is trying to do
    Ruling ruling;                // scope decision — see above; not the roll outcome

    std::vector<Modifier> modifiers;  // feeds the engine's roll; empty => no roll, success is automatic

    std::vector<Effect> effects_on_success;  // applied if the roll succeeds (or unconditionally if no roll)
    std::string narration_on_success;

    std::vector<Effect> effects_on_failure;  // applied if the roll fails; empty when modifiers is empty
    std::string narration_on_failure;
};

}  // namespace gm

// ---------------------------------------------------------------------
// Worked examples, as a compile-time sanity check that the vocabulary
// covers the spec's own worked examples (gm-adjudicator.md §4).
// ---------------------------------------------------------------------
namespace gm::examples {

// Bribe (partial scope, with a roll): 15 gold for the key. The GM already
// refused "abandon his post" at the scope level (ruling = partial) —
// that's fixed regardless of how the roll goes. The roll only decides
// whether the guard takes the bribe at all.
inline WorkOrder bribe_the_guard() {
    return WorkOrder{
        .interpretation = "Player offers the guard 15 gold for the key.",
        .ruling = Ruling::Partial,  // "hand over the key" is negotiable; "abandon post" was refused at scope, not by the roll
        .modifiers = {{"bribable", 2}, {"generous_offer", 3}},

        .effects_on_success = {
            ModifyResource{{"player"}, ModifyResource::Resource::Gold, -15},
            TransferItem{{"key_north"}, {"guard_01"}, {"player"}},
            ModifyDisposition{{"guard_01"}, +1},
        },
        .narration_on_success =
            "The guard pockets the coin and slides you the key — but he's not budging from his post.",

        .effects_on_failure = {
            ModifyDisposition{{"guard_01"}, -1},
        },
        .narration_on_failure =
            "The guard shoves the coin back at you, insulted. \"Try that again and I'll call the captain.\"",
    };
}

// Burn the door: no `burn` primitive; maps onto existing primitives.
// No roll (modifiers empty) — success is unconditional, no failure branch.
inline WorkOrder burn_the_door() {
    return WorkOrder{
        .interpretation = "Player pours the oil flask on the door and lights it.",
        .ruling = Ruling::Allow,
        .modifiers = {},

        .effects_on_success = {
            SetObjectState{{"door_north"}, "locked", false},
            TransferItem{{"oil_flask"}, {"player"}, VOID_ENTITY},
        },
        .narration_on_success = "The oak catches fast; the lock gives way in a curl of smoke.",
        // no roll => no failure branch
    };
}

// Hallucination: references an item id that doesn't exist in state.
// This WorkOrder is schema-valid — the rejection happens one gauntlet
// stage later, at the validator's existence check, not here.
inline WorkOrder hallucinated_item() {
    return WorkOrder{
        .interpretation = "Player demands the guard's silver key.",
        .ruling = Ruling::Allow,
        .modifiers = {},

        .effects_on_success = {
            TransferItem{{"silver_key"}, {"guard_01"}, {"player"}},  // silver_key does not exist
        },
        .narration_on_success = "The guard hands over a key.",
    };
}

}  // namespace gm::examples
