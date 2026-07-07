#include "gm_gauntlet.hpp"
#include "gm_json.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>

static const char* kWorkOrderPath = "workorder_turn.json";

static std::string slurp(const char* path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
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
            std::cout << "Turn completed: " << (turn_number - 1) << "\n";
            break;
        }

        // trim + lowercase, then compare to "quit"
        std::string trimmed = line;
        auto not_space = [](unsigned char c) { return !std::isspace(c); };
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
        trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
        std::string lowered = trimmed;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return std::tolower(c); });
        if (lowered == "quit") {
            std::cout << "\n=== SESSION ENDED (quit) ===\n";
            std::cout << "Turn completed: " << (turn_number - 1) << "\n";
            break;
        }
    }

    return 0;
}
