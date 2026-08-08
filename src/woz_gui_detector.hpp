#pragma once
#include <string>

#include "gm_state.hpp"

// Tier-0 detector for the woz_gui compositor (woz-gui-orchestrator-flow.md,
// Phase 2). Pure classification: does a composed kind+target resolve inline
// (no WorkOrder/GM involved) or route to the adjudicated path? Split into
// its own header, rather than living as static functions inside
// woz_gui.cpp, so it links against a standalone test binary
// (test_woz_gui_detector.cpp) like the project's other test_*.cpp targets.

namespace gm::woz_gui {

// Looks up world.room.exits for direction, resolves the gating
// ObjectState's "locked" property, returns !locked. Missing exit, missing
// object, or a non-bool/absent "locked" property all default to blocking
// (false) -- the detector only ever decides bypass-or-not, never
// legality, so the safe default is "needs the GM."
inline bool exit_permits_passage(const World& world, const std::string& direction) {
    auto exit_it = world.room.exits.find(direction);
    if (exit_it == world.room.exits.end())
        return false;

    for (const auto& object : world.objects) {
        if (object.id != exit_it->second.object)
            continue;
        auto prop_it = object.properties.find("locked");
        if (prop_it == object.properties.end())
            return false;
        const bool* locked = std::get_if<bool>(&prop_it->second);
        return locked && !*locked;
    }
    return false;  // gating object missing
}

// Encodes the spec's locked per-kind rule table -- examine always true;
// move defers to exit_permits_passage; interact/use_item_on/attack/say
// always false; any other kind string false (silent safe default).
inline bool is_tier0(const std::string& kind, const World& world, const std::string& target) {
    if (kind == "examine")
        return true;
    if (kind == "move")
        return exit_permits_passage(world, target);
    return false;
}

}  // namespace gm::woz_gui
