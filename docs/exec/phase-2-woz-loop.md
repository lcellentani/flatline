# Exec Plan: Phase 2 — Headless CLI Multi-Turn Loop (`woz_loop.cpp`)

**Source plan:** `docs/plans/gm-multiturn-loop.md`, Phase 2, Task 2.1 (currently "Not Started").

## Context

Phase 0 (toolchain) and Phase 1 (shared `gm_turn::process_turn` pipeline) are Done. `wizard_of_oz_demo.cpp` still only runs one hardcoded turn — nothing built so far produces persistent, multi-turn state, which is what's needed to generate real Experiment-1 ("is it fun?") data. Phase 2 closes that gap with a plain stdin/stdout CLI loop: load a room once, then repeatedly read a live-edited `workorder_turn.json`, run it through the already-proven `process_turn`, print the outcome, and carry `World` forward until the player dies or types `quit`. This is also the first scriptable regression-run entry point (a wrapper script can pre-stage WorkOrder files and pipe blank lines to auto-advance turns).

## 1. New file: `src/woz_loop.cpp`

Includes: `gm_gauntlet.hpp`, `gm_json.hpp`, `gm_log.hpp`, `gm_roll.hpp`, `gm_rules.hpp`, `gm_turn.hpp`, `<fstream>`, `<iostream>`, `<optional>`, `<sstream>`, `<string>`, `<algorithm>`, `<cctype>`.

```cpp
static const char* kLogPath = "session_log.jsonl";
static const char* kWorkOrderPath = "workorder_turn.json";

static std::string slurp(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
```

Duplicates `wizard_of_oz_demo.cpp`'s local static `slurp` verbatim rather than extracting a shared helper — 5 lines, nothing to keep in sync, and Phase 3's `woz_gui.cpp` (arguably not even needing a file-slurp, since ImGui text boxes hold content in memory) is a more natural trigger for a shared `gm_io.hpp` if one is ever justified. Flag as a follow-up when Phase 3 starts; don't extract now.

### `main()` structure

```cpp
int main(int argc, char** argv) {
    using namespace gm;

    const char* room_path = (argc > 1) ? argv[1] : "room_guardroom.json";

    World world;
    try {
        world = parse_world(slurp(room_path));
    } catch (const std::exception& ex) {
        std::cerr << "fatal: failed to load room '" << room_path << "': " << ex.what() << "\n";
        return 1;
    }

    int64_t turn_number = 1;

    while (true) {
        std::cout << "=== TURN " << turn_number << " ===\n";
        std::cout << dump(world) << "\n\n";
        std::cout << "Edit " << kWorkOrderPath << " with the ruling, save it, then press Enter.\n";
        std::cout << "(or type 'quit' to end the session)\n> ";

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << "\n=== SESSION ENDED (stdin closed) ===\n";
            std::cout << "Turns completed: " << (turn_number - 1) << "\n";
            break;
        }

        // trim + lowercase, then compare to "quit"
        std::string trimmed = line;
        auto not_space = [](unsigned char c) { return !std::isspace(c); };
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
        trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
        std::string lowered = trimmed;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                        [](unsigned char c) { return std::tolower(c); });

        if (lowered == "quit") {
            std::cout << "\n=== SESSION ENDED (quit) ===\n";
            std::cout << "Turns completed: " << (turn_number - 1) << "\n";
            break;
        }

        WorkOrder wo;
        try {
            std::string raw = slurp(kWorkOrderPath);
            if (raw.empty()) {
                std::cout << "\n" << kWorkOrderPath
                          << " is missing or empty -- write a WorkOrder and press Enter again.\n\n";
                continue;  // re-prompt same turn_number, world unchanged, nothing logged
            }
            wo = parse_work_order(raw);
        } catch (const std::exception& ex) {
            std::cout << "\ncould not parse " << kWorkOrderPath << ": " << ex.what()
                      << "\n(turn_number unchanged -- fix the file and press Enter again)\n\n";
            continue;  // re-prompt same turn_number, world unchanged, nothing logged
        }

        auto [world_after, outcome] = turn::process_turn(world, turn_number, wo, kLogPath);
        print_outcome(wo, world, world_after, outcome);
        world = world_after;

        if (std::holds_alternative<log::Applied>(outcome) && rules::session_over(world)) {
            std::cout << "\n=== SESSION OVER: player is dead ===\n";
            std::cout << "Turns completed: " << turn_number << "\n";
            break;
        }

        ++turn_number;
    }

    return 0;
}
```

### `print_outcome` helper (file-local, above `main`)

Mirrors `wizard_of_oz_demo.cpp`'s existing outcome-printing shape (roll die/modifier_sum/total vs `kDifficultyClass`; `Applied` → narration + effect count + gold/hp/inventory deltas; `GauntletRejected` → effect_index/stage/reason; `ModifierBudgetRejected` → reason), dropping the demo's one-time round-trip/vocabulary-gate prints (those are demo-only exercises, not loop behavior).

```cpp
static void print_outcome(const WorkOrder& wo,
                           const World& world_before,
                           const World& world_after,
                           const log::Outcome& outcome) {
    if (auto* budget_failure = std::get_if<log::ModifierBudgetRejected>(&outcome)) {
        std::cout << "=== MODIFIER BUDGET ===\nREJECTED: " << budget_failure->reason << "\n\n";
        return;
    }
    std::cout << "=== MODIFIER BUDGET ===\nOK\n\n";

    bool rolled = !wo.modifiers.empty();
    const std::optional<log::RollInfo>* roll_info =
        std::holds_alternative<log::GauntletRejected>(outcome)
            ? &std::get<log::GauntletRejected>(outcome).roll
            : &std::get<log::Applied>(outcome).roll;

    if (rolled && *roll_info) {
        const auto& r = (*roll_info)->result;
        std::cout << "=== ROLL ===\ndie " << r.die << " + modifiers " << r.modifier_sum
                   << " = " << r.total << " vs DC " << rules::kDifficultyClass
                   << " -> " << (r.success ? "SUCCESS" : "FAILURE") << "\n\n";
    } else {
        std::cout << "=== ROLL ===\nno modifiers -> automatic success\n\n";
    }

    if (auto* applied = std::get_if<log::Applied>(&outcome)) {
        std::cout << "=== NARRATION ===\n" << applied->narration << "\n\n";
        std::cout << "=== APPLIED ===\n" << applied->applied_effects.size() << " effect(s) applied\n";
        std::cout << "player.gold : " << world_before.player.gold << " -> " << world_after.player.gold << "\n";
        std::cout << "player.hp   : " << world_before.player.hp << " -> " << world_after.player.hp << "\n";
        std::cout << "player.inventory size : " << world_before.player.inventory.size()
                   << " -> " << world_after.player.inventory.size() << "\n\n";
    } else {
        const auto& f = std::get<log::GauntletRejected>(outcome);
        std::cout << "=== GAUNTLET ===\nREJECTED at effect " << f.effect_index
                   << " [" << f.stage << "]: " << f.reason << "\n\n";
    }
}
```

## 2. Failure-handling policy (missing/malformed `workorder_turn.json`)

**Decision: catch and re-prompt the same turn — do not advance `turn_number`, do not touch `World`, do not call `process_turn` (so nothing is logged).**

- Phase 2's acceptance criteria only describe the happy path + `quit`; crashing on a missing `workorder_turn.json` at `turn_number == 1` (before the tester has created it) would make AC #1's own walkthrough impossible to run without a lucky first attempt.
- Phase 3's plan already assigns "catch malformed JSON, don't advance the turn" to its GUI Submit handler as *new* behavior, reasoning that the CLI/demo "only ever read well-formed files off disk." Phase 2 breaks that premise — `woz_loop.cpp` is the first thing that reads a file that might not exist yet or might be mid-edit. So Phase 2 needs the same *policy* (catch, report, don't advance) with simpler *rendering* (plain stdout text, no GUI banner) — worth locking now so Phase 3 doesn't invent an inconsistent policy independently.
- `parse_work_order` throws `std::runtime_error` for schema/vocabulary violations; `nlohmann::json::parse` throws `json::parse_error` (a `std::exception` subclass) for invalid JSON — one `catch (const std::exception&)` covers both. An empty read (file doesn't exist / failed to open) is checked separately first, with a clearer message than a raw JSON parse error, since it's the most likely case at turn 1 for a fresh tester.
- The room load (`argv[1]`, once at startup) stays **fatal** (`return 1`) — there's no sensible "re-prompt" for a scenario file that fails before any `World` exists, matching the demo's own unguarded `parse_world(slurp(...))` call.

## 3. `CMakeLists.txt` diff

```diff
 flatline_add_executable(test_turn src/test_turn.cpp)
 flatline_add_executable(woz src/wizard_of_oz_demo.cpp)
+flatline_add_executable(woz_loop src/woz_loop.cpp)
```

No changes to `FLATLINE_JSON_FIXTURES` — the existing list (`room_guardroom.json`, `workorder_bribe.json`, `workorder_hallucinated.json`) is already copied post-build into `woz_loop`'s output dir, covering the default room load. No new libraries or toolchain changes.

## 4. Fixture/seed file decision

Do **not** add `workorder_turn.json` to `FLATLINE_JSON_FIXTURES` — it's the one file in this plan meant to be user-edited between runs; copy-if-different would silently clobber a tester's in-progress edit on every rebuild, and it would get copied into `woz`'s and every test binary's output dir for no reason.

For manual testing, seed it by copying an existing tracked fixture (`workorder_bribe.json` → `workorder_turn.json`) as a one-line manual step in the verification walkthrough below — this is also literally what AC #1 itself specifies. No new tracked file, no CMake change needed.

## 5. Manual verification walkthrough (run from `build/bin`)

**AC #1 — one applied turn identical to the demo's, logged as `turn_number == 1`:**
1. `Copy-Item workorder_bribe.json workorder_turn.json`
2. Run `./woz_loop`; observe `=== TURN 1 ===` + `dump(world)`.
3. Press Enter.
4. Confirm output matches the demo's applied-turn shape (roll vs DC 15, narration, `player.gold: 15 -> 0`, inventory unchanged size, item transfer visible in next `dump`/log).
5. Inspect `session_log.jsonl`: exactly one line, `"turn_number": 1`, `Applied`-shaped.
6. Type `quit`.

**AC #2 — two log entries, turn 2's `world_before == ` turn 1's `world_after`:**
1. Fresh run (clear/rename any prior `session_log.jsonl`).
2. Seed + apply turn 1 as above.
3. Before pressing Enter for turn 2, edit `workorder_turn.json` to a second valid, distinct ruling. Save, press Enter.
4. Confirm two `session_log.jsonl` lines (`turn_number` 1 and 2); parse both and confirm `entry[1].world_before == entry[0].world_after` structurally.
5. Type `quit`.

**AC #3 — session ends after crossing `kDeathOvershoot` (-5), no further prompt:**
1. Author `workorder_turn.json` with a `modify_resource(player, "hp", delta)` effect (uncapped, unlike `damage`'s `kDamageCap` per weapon) sized to take `player.hp` (starts at 20) to ≤ -5 net.
2. Drive turns one at a time until the applying turn crosses the threshold.
3. Confirm `=== SESSION OVER: player is dead ===` prints immediately after that turn, no further `=== TURN N ===` prompt, process exits 0 without blocking on stdin.
4. Confirm `session_log.jsonl`'s final entry shows the `Alive`/`Unconscious` → `Dead` `vital_transitions`, nothing beyond it.

**AC #4 — `quit` ends immediately without reading `workorder_turn.json`:**
1. Fresh run; delete any stale `workorder_turn.json` first.
2. At the first `=== TURN 1 ===` prompt, type `quit`.
3. Confirm clean exit (0), no parse-error about a missing file — proves `quit` short-circuits before `slurp`/`parse_work_order`.
4. Confirm `session_log.jsonl` untouched.

## 6. Exit codes

- `0`: normal termination — `quit`, EOF on stdin, or `session_over` (death). All three are expected endings for a scriptable loop; a wrapper script running an automated playtest to death shouldn't see that as a failure.
- `1`: fatal room-load failure only (`argv[1]` doesn't parse/exist).
- Malformed/missing `workorder_turn.json` never reaches a `return` — it re-prompts (§2).

## Files touched

- `src/woz_loop.cpp` (new)
- `CMakeLists.txt` (one line added)

## Verification (after implementation)

1. `cmake --build build --preset default` — `woz_loop` builds cleanly alongside the five existing targets, which must show unchanged pass counts (`test_gauntlet` 22/22, `test_log` 37/37, `test_applier` 31/31, `test_roll` 20/20, `test_turn` 18/18, `woz` exit 0).
2. Run the four AC walkthroughs in §5 against the built `woz_loop` binary.
