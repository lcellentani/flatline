# WorkOrder Rulebook — for the Human GM

| Field | Value |
|-------|-------|
| **Purpose** | Practical authoring reference for a human playing the GM role behind the validator (Experiment 1) |
| **Contract** | `docs/specs/gm-adjudicator.md` (this doc restates it for authoring convenience; the spec is authoritative if they ever disagree) |
| **Scope** | v0 guardroom room (`room_guardroom.json`) and its 6-primitive effect vocabulary |

---

## 1. What you're writing

Once per player turn, you fill out **one WorkOrder**. You never touch world state directly — you propose it, and the engine's gauntlet decides whether your proposal is legal before applying it. If any part of it is illegal, the *entire* turn is rejected and nothing changes; there's no partial credit.

A WorkOrder has three jobs, all done in one pass, before you know the roll's outcome:

1. **Interpret** what the player is trying to do, in your own words.
2. **Rule** how much of that is even on the table (`ruling`) and how hard it should be (`modifiers`).
3. **Author both outcomes** — what happens if it works, and what happens if it doesn't — as effects drawn only from the six primitives in §3.

The engine, not you, rolls the die, picks a branch, validates it, applies it, and logs it. Your job ends at authoring; nothing you write executes until it survives the gauntlet (§5).

## 2. The seven fields

```
interpretation        : one line — what the player is trying to do
ruling                : "allow" | "partial" | "deny"
modifiers             : [ { label, value } ]     // empty => no roll, success is automatic
effects_on_success    : [ <effect>, … ]          // applied if the roll succeeds (or unconditionally if no roll)
narration_on_success  : 1–2 sentences, your voice
effects_on_failure    : [ <effect>, … ]          // applied if the roll fails
narration_on_failure  : 1–2 sentences, your voice
```

**`interpretation`** — a plain restatement of player intent. Doesn't affect mechanics, but it's part of the replay log and it's what the roll's seed is hashed from (§5) — two GMs describing the same action with different wording *will* roll differently. Write it like you'd want to read it back later.

**`ruling`** — a *scope* decision, made independent of any dice: of what the player asked for, how much is even possible.
- `allow` — the whole stated intent is on the table.
- `partial` — part of it is refused outright, permanently, regardless of the roll. Use this only when you can point to the specific clause you refused. Example: "bribe the guard to open the door **and** abandon his post" → `partial`, because "abandon his post" is refused at the scope level; no roll will ever grant it.
- `deny` — none of it is on the table. Express this as empty `effects_on_success`/`effects_on_failure` plus a refusal in `narration_on_success` (there's no roll, so there's no "failure" to narrate separately).

Don't reach for `partial` just because the outcome is uncertain — uncertainty belongs in `modifiers`, not `ruling`. If nothing in the request is actually refused, it's `allow`, even if the roll might still fail.

**`modifiers`** — feeds the roll (§4). Each entry is `{ label, value }`; `value` can be positive or negative. **Empty `modifiers` means no roll at all** — `effects_on_success` applies unconditionally and `effects_on_failure` should stay empty (there's no failure branch to define).

**`effects_on_success` / `effects_on_failure`** — lists of effects (§3), applied atomically as a whole batch. If `modifiers` is non-empty, populate both — a live roll always has a real chance of landing on either branch, and an empty branch means "nothing happens," which almost never reads well as GM narration. Author both branches *before* you know how the roll lands; you don't get to see the result and then decide.

**`narration_on_success` / `narration_on_failure`** — 1–2 sentences in GM voice, describing the branch that was actually selected. Every resolved turn needs narration on whichever branch fires; a rejected-before-the-roll turn (over-budget modifiers) or a gauntlet rejection doesn't reach narration at all — that's a re-author, not a turn.

## 3. The effect vocabulary (six primitives, v0)

This is a **closed** vocabulary — an effect type outside this list is rejected immediately, before the gauntlet even runs. Novel player verbs get expressed as combinations of these six, not new primitives (see the `burn_the_door` example in §7).

### `damage(source, target, via, amount)`
Deals hp damage. `via` is **required** — name the specific item used (or `__unarmed__` for fists, §3.7). `amount` is clamped down to `via`'s cap in the closed damage table:

| item | cap |
|---|---|
| `dagger` | 4 |
| `oil_flask` | 2 |
| `key_north` | 1 |
| `__unarmed__` | 1 |

Every item that can exist in the room needs a row here — if you invent a new item and try to damage with it, that's a table gap for the engineers, not something you can work around by picking a different `via`. `source == target` is legal (self-damage — traps, poison).

> **Convention, not enforced:** if the hp loss comes from one entity acting on another, author it as `damage`, not `modify_resource(hp)` — that's the only way it stays subject to the weapon cap above. Reserve `modify_resource(hp)` for untargeted causes (see below). The validator can't catch you routing an attack through the wrong primitive; this is on you.

### `modify_disposition(entity, delta)`
Moves an entity's disposition along a clamped ordinal scale:

| value | label |
|---|---|
| −2 | Hostile |
| −1 | Unfriendly |
| 0 | Neutral |
| +1 | Friendly |
| +2 | Allied |

`delta` is clamped into this range — you can't overshoot it, so don't hesitate to write `-2` even if the entity's current disposition is already near the floor. **Not legal on `player`** — the player has no disposition field. Also illegal against a `Dead` entity (still legal against `Unconscious`).

### `transfer_item(item, from, to)`
Moves an item between holders. `from` must actually be holding `item` **right now** — checked against the state as of the start of this branch, folded forward through any earlier effects in the same batch (§5). `to = "__void__"` destroys/consumes the item — this is how you express "used up" (see `burn_the_door`, §7) without a separate `destroy_item` primitive. `from == to` is a legal no-op. Transferring from an `Unconscious` or `Dead` entity is deliberately allowed — looting a downed guard is meant to work.

### `modify_resource(actor, resource, delta)`
`resource` is `"gold"` or `"hp"`.
- **`gold`** is player-only (entities don't have gold). A negative delta that would take the player below 0 gold is **rejected outright**, not clamped — you can't spend money you don't have, same as `transfer_item`'s `from` check.
- **`hp`** works on both `player` and entities, and *is* clamped (floor 0, ceiling `max_hp`) rather than rejected — unlike gold, "take lethal damage" has to be a legal outcome. Reserve this for **untargeted** causes only (traps, potions) — see the `damage` convention above.
- Illegal against a `Dead` actor.

### `set_object_state(object, property, value)`
Sets a property on an object (e.g. `door_north.locked = false`). `property` must already exist in that object's seeded state (check the room fixture — properties aren't a separate schema, they're whatever the object was authored with), and `value`'s type must match the existing value's type (a bool property can't become a string). There's no "create a new property" path.

### `set_flag(key, value)`
Writes into a room-level memory bank. **Dormant in v0** — nothing reads flags back yet, so using this has no observable effect. It exists so a future memory feature doesn't need a new primitive; don't rely on it to gate anything today.

### Sentinels
Two reserved ids are *not* real state — never refer to them as if they were entities/items you could inspect:
- `__void__` — the "consumed/destroyed" sink for `transfer_item.to`.
- `__unarmed__` — "no weapon" for `damage.via`. `via` is never optional; this is the explicit value for a bare-handed hit, not a fallback the engine picks for you.

## 4. The roll

If `modifiers` is non-empty, the engine rolls **1d20 + Σ(modifier values)** against a fixed **DC 15**. Total ≥ 15 succeeds. This is legible at the table: "+5 needs a 10+," no arithmetic beyond addition.

**Modifier budget: Σ|value| ≤ 5**, checked against the *magnitude* (absolute values), not the net sum — so you can't stack an unbounded pile of negative modifiers to force a guaranteed failure either. This is enforced: a work order over budget is rejected before the roll ever runs, and the turn has to be re-authored. Spend is `≤`, not exact — one honestly-justified modifier doesn't need a second invented one just to fill the pool.

**Authoring guidance, not a mechanical rule:** each modifier should be a real, nameable circumstance from the fiction — "bribable," "generous offer on a 15-gold purse," "guard distracted." Don't reach for the budget ceiling to make an outcome likely; that's quietly doing the roll's job yourself. At the extremes: 5 is the largest single-direction budget that still keeps *both* directions of the roll genuinely uncertain (best case needs a 10+, ~55%; worst case needs a 20, ~5%) — spending it all in one line item is legal but usually means the fiction hasn't actually earned that much swing. If you find yourself doing this, either split the justification into two honest modifiers that sum to it, or — if the outcome really shouldn't be in doubt — drop `modifiers` to `[]` and let it succeed unconditionally instead of gaming a roll toward near-certainty.

**No modifiers ⇒ no roll.** `effects_on_success` applies unconditionally; there is no failure branch to author.

## 5. The gauntlet (what happens after you submit)

Two tiers, in order. A turn ends at whichever exit point it reaches, and exactly one log entry is written regardless of which:

1. **Modifier budget** (whole work order, before any roll): Σ|value| ≤ 5, or reject here.
2. **Roll**: no roll if `modifiers` empty (success automatic); otherwise d20 + modifier sum vs. DC 15 picks `effects_on_success` or `effects_on_failure`.
3. **Per effect, on the selected branch only** — first failure rejects the *whole batch*, nothing partially applies:
   - **ids exist** — every id you referenced is real in current state (this is the hallucination check; sentinels are carved out).
   - **preconditions** — checked against state *as folded forward through this branch's own earlier effects*, not just the turn's starting snapshot. Two effects in the same batch that individually look legal but conflict with each other (two `transfer_item`s claiming the same item; two gold spends that don't fit together) are rejected as a **batch conflict**, naming the earlier effect responsible — distinct from an ordinary precondition that was never legal.
   - **magnitude bounds** — `damage` clamped to its cap, `modify_disposition` clamped to its enum range (reading the same folded state, so a batch's own effects compound correctly).
4. **Apply atomically**, then **log** — one entry either way, whatever stage it reached.

Practical upshot for authoring: order the effects within a branch to match cause-and-effect (e.g. take the item before you'd need it gone), and never let two effects in the same branch stake a claim on the same resource unless you mean for the second to see the first's result.

## 6. Pre-submit checklist

- [ ] Does every `from` in `transfer_item` (and every `via`/`source` in `damage`) actually hold that item/exist **right now**, including anything an earlier effect in this same branch already moved?
- [ ] Is this hp change caused by one entity acting on another? If so, is it `damage`, not `modify_resource(hp)`?
- [ ] Is `modify_disposition`'s target an entity, never `player`?
- [ ] Does a negative `gold` delta actually fit the player's current balance?
- [ ] Are your `modifiers` each individually justified by the fiction — not padded to force a favorable roll?
- [ ] If `modifiers` is non-empty, are **both** branches populated with effects and narration?
- [ ] Does `ruling = partial` name a specific clause you refused? If nothing is actually refused, is this really `allow`?
- [ ] Any new item introduced needs a row in the damage-cap table (or nothing can ever deal damage with it) — flag this to the engineers rather than substituting a different `via`.

## 7. Worked examples

**Bribe (`workorder_bribe.json`) — partial scope, with a roll.**
Intent: "bribe the guard with 15 gold for the key." `ruling = partial` — handing over the key is negotiable, but "abandon his post" (if asked) is refused at scope, not by the roll. Modifiers `[bribable +2, generous_offer +3]`, magnitude 5. On success: pay the gold, receive the key, guard's disposition +1. On failure: guard's disposition −1, coin stays with the player.

**Open the door (`workorder_open-door.json`) — a later turn in the same session, depending on the bribe's outcome.**
Because the bribe's success branch already transferred `key_north` to the player, this turn's `transfer_item` legally reads `from: "player"`. Modifiers `[guard_distracted +2, player_moves_quietly +2]`, magnitude 4 — under budget on purpose, split into two honest reasons rather than one maxed-out line. On success: door unlocks, key is consumed (`to: "__void__"`). On failure: the guard catches the player mid-attempt — key transfers back to `guard_01`, disposition drops −2. This is a good template for any "use what you already have" turn: check what a *previous* turn's success branch left you holding before writing `from`.

**Burn the door — novel verb, no roll, expressed in existing primitives.**
Intent: "pour the oil flask on the door and light it." No primitive named `burn`; it decomposes into `set_object_state(door_north, locked, false)` + `transfer_item(oil_flask, player, "__void__")` (the flask is consumed). `modifiers` empty ⇒ unconditional success, no failure branch to write.

**Hallucination — a trap to avoid, not a pattern to follow.**
A work order proposing `transfer_item(silver_key, guard_01, player)` when no `silver_key` exists in state is *schema-valid* — it parses fine — but fails the ids-exist stage. Before naming an id in an effect, check it against the state snapshot you were actually handed, not what you assume should be there.

**Over-budget modifiers — also a trap.**
`[bribable +2, generous +3, desperate +3]` sums to magnitude 8, over the budget of 5. Rejected before the roll ever runs; nothing in state changes and the turn must be re-authored with modifiers brought back under budget.

---

*This is a living reference for v0's single guardroom and its 6-primitive vocabulary — both will grow. If you hit a player intent that genuinely doesn't decompose into these primitives, that's a signal for the effect vocabulary to grow (see `gm-adjudicator.md` §3 Non-Goals), not a reason to bend an existing primitive's meaning.*
