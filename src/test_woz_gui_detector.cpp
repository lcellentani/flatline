// Verification harness for woz_gui_detector.hpp -- not a project artifact,
// just the compile+behavior gate for the woz-gui-orchestrator-flow plan's
// Phase 2 (Tier-0 detector). Covers exit_permits_passage (Task 2.1);
// is_tier0 (Task 2.2) is still a declaration only, so it isn't exercised
// here yet.
//
// Build: g++ -std=c++20 -I. -I<path-to-nlohmann> -Wall -Wextra test_woz_gui_detector.cpp -o test_woz_gui_detector

#include <fstream>
#include <iostream>
#include <sstream>

#include "gm_json.hpp"
#include "woz_gui_detector.hpp"

using namespace gm;

static std::string slurp(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        std::cout << "PASS  " << name << "\n";
        ++g_pass;
    } else {
        std::cout << "FAIL  " << name << "\n";
        ++g_fail;
    }
}

static ObjectState* find_object(World& w, const char* id) {
    for (auto& o : w.objects)
        if (o.id == ObjectId{id}) return &o;
    return nullptr;
}

int main() {
    // room_guardroom.json: exit "north" gated by door_north, locked: true.
    World base = parse_world(slurp("room_guardroom.json"));

    // --- exit_permits_passage -----------------------------------------

    check(woz_gui::exit_permits_passage(base, "north") == false,
          "exit_permits_passage: locked door_north -> false");

    {
        World world = base;
        find_object(world, "door_north")->properties["locked"] = false;
        check(woz_gui::exit_permits_passage(world, "north") == true,
              "exit_permits_passage: unlocked door_north -> true");
    }

    check(woz_gui::exit_permits_passage(base, "south") == false,
          "exit_permits_passage: nonexistent direction -> false, no throw");

    {
        World world = base;
        world.objects.clear();
        check(woz_gui::exit_permits_passage(world, "north") == false,
              "exit_permits_passage: missing gating object -> false");
    }

    {
        World world = base;
        find_object(world, "door_north")->properties.erase("locked");
        check(woz_gui::exit_permits_passage(world, "north") == false,
              "exit_permits_passage: absent locked property -> false");
    }

    {
        World world = base;
        find_object(world, "door_north")->properties["locked"] = std::string("yes");
        check(woz_gui::exit_permits_passage(world, "north") == false,
              "exit_permits_passage: non-bool locked property -> false");
    }

    std::cout << "\n" << g_pass << " passed, " << g_fail << " failed\n";
    return g_fail == 0 ? 0 : 1;
}
