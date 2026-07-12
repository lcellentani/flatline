#pragma once
#include "gm_effects.hpp"  // Id<>, EntityId/ObjectId/ItemId, PropertyValue
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// GM Adjudicator — v0 engine-owned state model.
// Realizes gm-adjudicator.md §4 "Engine-owned state (the source of truth)".
// This is the deterministic engine's truth; the GM never mutates it — the
// validator applies validated deltas to it. Defined here (rather than in the
// effects header) because the serialization bridge needs a state *type* to
// read a room from disk and to show the current state to the human GM.

namespace gm {

// Disposition is an ordinal scale; modify_disposition's delta moves along it
// and the validator clamps to [Hostile, Allied]. Stored as an int so the
// delta arithmetic is well-defined; the JSON bridge maps to/from the labels
// below so authored rooms and state dumps stay human-legible.
namespace disposition {
    inline constexpr int Hostile    = -2;
    inline constexpr int Unfriendly = -1;
    inline constexpr int Neutral    =  0;
    inline constexpr int Friendly   =  1;
    inline constexpr int Allied     =  2;
}

struct Exit {
    ObjectId object;  // the object gating this exit, e.g. door_north
};

struct ObjectState {
    ObjectId id;
    std::map<std::string, PropertyValue> properties;  // e.g. "locked"->bool, "material"->string
};

// Vital state — a third axis of entity state, alongside disposition
// (attitude) and hp (a number): what an entity currently *is*, not how it
// feels or how much health remains (gm-adjudicator.md §5, "hp ceiling and
// alive/dead state"). A flat enum, not a std::variant, deliberately: the
// two carry identical safety once a transition-legality check exists
// (gm_rules.hpp::derive_vital_state) — nothing here needs per-state
// payload yet. A concrete future consumer (overkill magnitude as cause-of-
// death narration color) is the named trigger to revisit that choice, not
// before, per this project's own "second use case earns the structure"
// discipline. Also the known limit of a flat enum, worth stating rather
// than discovering later: cheap to add more *mutually exclusive* states,
// cannot represent states *co-occurring* (e.g. poisoned AND unconscious)
// without a structural change.
enum class VitalState : uint8_t { Alive, Unconscious, Dead };

struct Entity {
    EntityId id;
    int64_t hp;
    int64_t max_hp;               // ceiling: gm_applier.hpp clamps hp to [0, max_hp]
    VitalState vital = VitalState::Alive;
    int32_t disposition;         // ordinal; see disposition:: above
    std::vector<ItemId> holds;
    std::string persona;         // free text the GM reads; never mechanically parsed
};

struct Player {
    EntityId id;
    int64_t hp;
    int64_t max_hp;
    VitalState vital = VitalState::Alive;
    int64_t gold;
    std::vector<ItemId> inventory;
};

struct Room {
    std::string id;
    std::map<std::string, Exit> exits;  // key: direction, e.g. "north"
};

struct World {
    Room room;
    std::vector<ObjectState> objects;
    std::vector<Entity> entities;
    Player player;

    // Memory hook storage (context doc §4: "adjudication and memory are a
    // stack, not a fork"). SetFlag (gm_effects.hpp) was dormant everywhere
    // up to session log 6 — the gauntlet always passes it, but nothing in
    // World had a place for the write to land. Added here rather than left
    // implicit, per this project's own "deferred gaps must be named, not
    // silently ignored" discipline (the same shape as Damage.via's fix):
    // an effect type that exists in the vocabulary but writes into the
    // void at apply time is a silent bypass waiting to happen.
    std::map<std::string, PropertyValue> flags;
};

}  // namespace gm
