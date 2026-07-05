#pragma once
#include "gm_effects.hpp"
#include "gm_json.hpp"
#include "gm_rules.hpp"
#include <cstdint>
#include <random>
#include <string>

// GM Adjudicator — v0 seeded roll.
// Realizes gm-adjudicator.md §5 "modifier -> roll scheme" (LOCKED this
// session as Approach A: a d20 roll plus the GM's modifier sum, checked
// against a fixed, engine-owned kDifficultyClass — gm_rules.hpp). Closes
// the spec's last open contract question before [ROLL] can be wired into
// the demo loop.
//
// Locked shape:
//   seed  = FNV-1a(turn_number, full WorkOrder JSON dump)
//   die   = 1..20, drawn from a std::mt19937_64 seeded with that hash
//   total = die + sum(modifier.value)     // NOT magnitude — see note below
//   success = total >= kDifficultyClass
//
// "Engine owns the dice" (spec, Key Decisions) is enforced by construction
// here: nothing in this header accepts a GM-authored target or a
// GM-authored outcome. The GM's only input is `modifiers`, and even that
// is bounded upstream by gm_gauntlet.hpp's validate_modifiers stage
// (kModifierBudget) before it ever reaches resolve() — this header trusts
// that precondition rather than re-deriving it, same posture gm_applier.hpp
// takes toward run_gauntlet's output.
//
// Modifier sum here is the SIGNED net sum (mirrors the effect on the roll:
// a -3 modifier should hurt the total, not add magnitude to it) —
// deliberately distinct from validate_modifiers' MAGNITUDE sum, which
// bounds how much total influence a WorkOrder may propose in either
// direction. Two different questions, two different aggregations of the
// same list: "how much is proposed" (magnitude, gauntlet stage) vs. "which
// way does it push the roll" (signed sum, this header).
//
// Hash choice: FNV-1a, not std::hash. std::hash<T> is UNSPECIFIED across
// compilers/stdlib versions/builds (notably for std::string on several
// implementations) — using it here would silently break the one property
// this whole scheme exists for: replaying a logged run later, possibly on
// a different machine, and getting the identical roll. FNV-1a is a small,
// fixed, well-known non-cryptographic hash with no such variance.
//
// Seed material: turn_number (engine-tracked, NOT a WorkOrder field — the
// harness/log owns turn sequencing, not the contract) concatenated with
// the WorkOrder's own canonical JSON dump (gm_json.hpp). This means two
// WorkOrders that differ only in prose (interpretation/narration text)
// produce different rolls, same as differing in any mechanical field — a
// deliberate simplicity choice (hash the whole dump, not a curated
// subset) rather than a claim that prose *should* matter narratively.

namespace gm::roll {

// ---------------------------------------------------------------------
// FNV-1a, 64-bit. Fixed offset basis and prime (the standard constants —
// not tunable, not project-specific) so the hash is portable and stable
// across machines and builds, which std::hash does not guarantee.
// ---------------------------------------------------------------------
inline uint64_t fnv1a_64(const std::string& data) {
    constexpr uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr uint64_t kPrime = 1099511628211ULL;
    uint64_t hash = kOffsetBasis;
    for (unsigned char byte : data) {
        hash ^= byte;
        hash *= kPrime;
    }
    return hash;
}

// Combines an externally-tracked turn number with the full WorkOrder
// content into one deterministic seed. turn_number is a parameter, not a
// WorkOrder field, per the note above — sequencing is the caller's
// (eventually the logger's) responsibility, not the contract's.
inline uint64_t seed_for_turn(int64_t turn_number, const WorkOrder& work_order) {
    std::string material = std::to_string(turn_number) + "|" + json(work_order).dump();
    return fnv1a_64(material);
}

// ---------------------------------------------------------------------
// The roll result. All fields kept, not just `success` — die and total
// are what a replay log needs to show its work (spec §4 gauntlet: "log
// the raw work order plus the applied deltas and the RNG seed/result"),
// and modifier_sum is echoed back so a failure/success is self-explaining
// without re-deriving it from wo.modifiers at every call site.
// ---------------------------------------------------------------------
struct RollResult {
    int32_t die;            // 1..20, the raw draw
    int32_t modifier_sum;   // signed net sum of wo.modifiers
    int32_t total;          // die + modifier_sum
    bool success;           // total >= kDifficultyClass
};

// Pure primitive: same (seed, modifier_sum) in, same RollResult out,
// forever — the actual property the replay guarantee rests on. Kept
// separate from resolve_turn() below so it's trivially unit-testable
// without needing a WorkOrder at all.
inline RollResult resolve(uint64_t seed, int32_t modifier_sum) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int32_t> d20(1, 20);
    int32_t die = d20(rng);
    int32_t total = die + modifier_sum;
    bool success = total >= rules::kDifficultyClass;
    return RollResult{die, modifier_sum, total, success};
}

// Convenience wrapper: hashes the seed and sums the modifiers so the
// caller (the demo loop, eventually the real harness) doesn't have to.
// Precondition, not enforced here (same division of labor gm_applier.hpp
// already draws toward run_gauntlet): wo.modifiers should already have
// passed gauntlet::validate_modifiers. Calling this on an un-validated
// WorkOrder is a caller bug, not something this header defends against.
inline RollResult resolve_turn(int64_t turn_number, const WorkOrder& work_order) {
    int32_t modifier_sum = 0;
    for (const auto& m : work_order.modifiers) modifier_sum += m.value;
    return resolve(seed_for_turn(turn_number, work_order), modifier_sum);
}

}  // namespace gm::roll
