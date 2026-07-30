#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>

#include "gm_effects.hpp"
#include "gm_json.hpp"
#include "gm_log.hpp"
#include "gm_rules.hpp"
#include "gm_turn.hpp"

static const char* kLogPath = "session_log.jsonl";
static const char* kWorkOrderPath = "workorder_turn.json";

static std::string slurp(const char* path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static void print_outcome(const gm::WorkOrder& wo, const gm::World& world_before, const gm::World& world_after,
                          const gm::log::Outcome& outcome) {
    using namespace gm;

    if (auto* budget_failure = std::get_if<log::ModifierBudgetRejected>(&outcome)) {
        std::cout << "=== MODIFIER BUDGET ===\nREJECTED: " << budget_failure->reason << "\n\n";
        return;
    }
    std::cout << "=== MODIFIER BUDGET ===\nOK\n\n";

    bool rolled = !wo.modifiers.empty();
    const std::optional<log::RollInfo>* roll_info = std::holds_alternative<log::GauntletRejected>(outcome)
                                                        ? &std::get<log::GauntletRejected>(outcome).roll
                                                        : &std::get<log::Applied>(outcome).roll;
    if (rolled && *roll_info) {
        const auto& r = (*roll_info)->result;
        std::cout << "=== ROLL ===\ndie " << r.die << " + modifiers " << r.modifier_sum << " = " << r.total << " vs DC "
                  << rules::kDifficultyClass << " -> " << (r.success ? "SUCCESS" : "FAILURE") << "\n\n";
    } else {
        std::cout << "=== ROLL ===\nno modifiers -> automatic success\n\n";
    }

    if (auto* applied = std::get_if<log::Applied>(&outcome)) {
        std::cout << "=== NARRATION ===\n" << applied->narration << "\n\n";
        std::cout << "=== APPLIED ===\n" << applied->applied_effects.size() << " effect(s) applied\n";
        std::cout << "player.gold : " << world_before.player.gold << " -> " << world_after.player.gold << "\n";
        std::cout << "player.hp   : " << world_before.player.hp << " -> " << world_after.player.hp << "\n";
        std::cout << "player.inventory size : " << world_before.player.inventory.size() << " -> "
                  << world_after.player.inventory.size() << "\n\n";
    } else {
        const auto& f = std::get<log::GauntletRejected>(outcome);
        std::cout << "=== GAUNTLET ===\nREJECTED at effect " << f.effect_index << " [" << f.stage << "]: " << f.reason
                  << "\n\n";
    }
}

int main(int argc, char* argv[]) {
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
        std::cout << dump(world) << "\n";
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
                std::cout << "\n"
                          << kWorkOrderPath << " is missing or empty -- write a WorkOrder and press Enter again.\n\n";
                continue;
            }
            wo = parse_work_order(raw);
        } catch (const std::exception& ex) {
            std::cout << "\ncould not parse " << kWorkOrderPath << ": " << ex.what()
                      << "\n(turn_number unchanged -- fix the file and press Enter again)\n\n";
            continue;
        }

        // No raw-input capture in this CLI front end (the GUI is the live
        // Experiment-1 tool) -- the log entry records an empty player_input.
        auto [world_after, outcome] = turn::process_turn(world, turn_number, wo, kLogPath, "");
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
